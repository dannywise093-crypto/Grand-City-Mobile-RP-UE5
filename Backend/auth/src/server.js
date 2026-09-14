import express from 'express';
import crypto from 'node:crypto';
import jwt from 'jsonwebtoken';
import pg from 'pg';

const { Pool } = pg;
const app = express();
app.use(express.json({ limit: '64kb' }));

const PORT = Number(process.env.PORT || 8080);
const DATABASE_URL = process.env.DATABASE_URL;
const JWT_SECRET = process.env.JWT_SECRET;
const INTERNAL_API_KEY = process.env.INTERNAL_API_KEY;

if (!DATABASE_URL || !JWT_SECRET || !INTERNAL_API_KEY) {
  throw new Error('DATABASE_URL, JWT_SECRET and INTERNAL_API_KEY are required');
}

const pool = new Pool({ connectionString: DATABASE_URL, max: 20 });

function requireInternal(req, res, next) {
  if (req.get('x-internal-api-key') !== INTERNAL_API_KEY) {
    return res.status(401).json({ error: 'unauthorized' });
  }
  next();
}

function issueToken(account) {
  return jwt.sign(
    { sub: account.account_id, displayName: account.display_name, regionId: account.region_id },
    JWT_SECRET,
    { issuer: 'grand-city-account-service', audience: 'grand-city-servers', expiresIn: '24h' }
  );
}

app.get('/health', async (_req, res) => {
  try {
    await pool.query('SELECT 1');
    res.json({ ok: true, service: 'account', time: new Date().toISOString() });
  } catch {
    res.status(503).json({ ok: false });
  }
});

app.post('/v1/accounts/register', async (req, res) => {
  const { displayName, password } = req.body ?? {};
  if (typeof displayName !== 'string' || displayName.length < 3 || displayName.length > 24 ||
      typeof password !== 'string' || password.length < 8 || password.length > 128) {
    return res.status(400).json({ error: 'invalid_registration' });
  }

  const accountId = crypto.randomUUID();
  const passwordHash = crypto.scryptSync(password, accountId, 64).toString('hex');

  try {
    const result = await pool.query(
      `INSERT INTO accounts(account_id, display_name, password_hash)
       VALUES ($1, $2, $3)
       RETURNING account_id, display_name, region_id`,
      [accountId, displayName, passwordHash]
    );
    const account = result.rows[0];
    await pool.query(
      `INSERT INTO player_profiles(account_id, character_id, character_name, region_id)
       VALUES ($1, $2, $3, $4)`,
      [account.account_id, `CHAR-${account.account_id}`, account.display_name, account.region_id]
    );
    res.status(201).json({ account, token: issueToken(account) });
  } catch (error) {
    if (error?.code === '23505') return res.status(409).json({ error: 'display_name_taken' });
    console.error(error);
    res.status(500).json({ error: 'registration_failed' });
  }
});

app.post('/v1/accounts/login', async (req, res) => {
  const { displayName, password } = req.body ?? {};
  if (typeof displayName !== 'string' || typeof password !== 'string') {
    return res.status(400).json({ error: 'invalid_login' });
  }

  const result = await pool.query(
    `SELECT account_id, display_name, region_id, password_hash FROM accounts WHERE display_name = $1`,
    [displayName]
  );
  const account = result.rows[0];
  if (!account) return res.status(401).json({ error: 'invalid_credentials' });

  const expected = crypto.scryptSync(password, account.account_id, 64).toString('hex');
  if (!crypto.timingSafeEqual(Buffer.from(expected), Buffer.from(account.password_hash))) {
    return res.status(401).json({ error: 'invalid_credentials' });
  }

  await pool.query(`UPDATE accounts SET last_login_at = NOW() WHERE account_id = $1`, [account.account_id]);
  res.json({ account: { account_id: account.account_id, display_name: account.display_name, region_id: account.region_id }, token: issueToken(account) });
});

app.post('/v1/auth/verify', requireInternal, async (req, res) => {
  try {
    const payload = jwt.verify(req.body?.token, JWT_SECRET, { issuer: 'grand-city-account-service', audience: 'grand-city-servers' });
    const result = await pool.query(
      `SELECT account_id, display_name, region_id FROM accounts WHERE account_id = $1`,
      [payload.sub]
    );
    if (!result.rows[0]) return res.status(401).json({ error: 'account_not_found' });
    res.json({ valid: true, account: result.rows[0] });
  } catch {
    res.status(401).json({ valid: false });
  }
});

app.post('/v1/transfers/lock', requireInternal, async (req, res) => {
  const { accountId, sourceServerId, targetServerId } = req.body ?? {};
  if (!accountId || !sourceServerId || !targetServerId || sourceServerId === targetServerId) {
    return res.status(400).json({ error: 'invalid_transfer' });
  }

  const client = await pool.connect();
  try {
    await client.query('BEGIN');
    const lock = await client.query(
      `SELECT account_id, active_server_id, transfer_token
       FROM account_sessions WHERE account_id = $1 FOR UPDATE`,
      [accountId]
    );
    if (!lock.rows[0] || lock.rows[0].active_server_id !== sourceServerId) {
      await client.query('ROLLBACK');
      return res.status(409).json({ error: 'source_session_not_active' });
    }
    const transferToken = crypto.randomUUID();
    await client.query(
      `UPDATE account_sessions
       SET transfer_token = $2, transfer_target_server_id = $3, transfer_started_at = NOW()
       WHERE account_id = $1`,
      [accountId, transferToken, targetServerId]
    );
    await client.query('COMMIT');
    res.json({ transferToken });
  } catch (error) {
    await client.query('ROLLBACK');
    console.error(error);
    res.status(500).json({ error: 'transfer_lock_failed' });
  } finally {
    client.release();
  }
});

app.post('/v1/sessions/claim', requireInternal, async (req, res) => {
  const { accountId, serverId, transferToken } = req.body ?? {};
  if (!accountId || !serverId) return res.status(400).json({ error: 'invalid_session' });

  const client = await pool.connect();
  try {
    await client.query('BEGIN');
    const result = await client.query(
      `SELECT transfer_token, transfer_target_server_id, active_server_id
       FROM account_sessions WHERE account_id = $1 FOR UPDATE`,
      [accountId]
    );
    const current = result.rows[0];
    if (current?.active_server_id && current.active_server_id !== serverId) {
      if (!transferToken || current.transfer_token !== transferToken || current.transfer_target_server_id !== serverId) {
        await client.query('ROLLBACK');
        return res.status(409).json({ error: 'account_already_active' });
      }
    }

    await client.query(
      `INSERT INTO account_sessions(account_id, active_server_id, transfer_token, transfer_target_server_id)
       VALUES ($1, $2, NULL, NULL)
       ON CONFLICT (account_id) DO UPDATE SET
         active_server_id = EXCLUDED.active_server_id,
         transfer_token = NULL,
         transfer_target_server_id = NULL,
         transfer_started_at = NULL,
         last_heartbeat_at = NOW()`,
      [accountId, serverId]
    );
    await client.query('COMMIT');
    res.json({ claimed: true });
  } catch (error) {
    await client.query('ROLLBACK');
    console.error(error);
    res.status(500).json({ error: 'session_claim_failed' });
  } finally {
    client.release();
  }
});

app.post('/v1/sessions/release', requireInternal, async (req, res) => {
  const { accountId, serverId } = req.body ?? {};
  const result = await pool.query(
    `DELETE FROM account_sessions WHERE account_id = $1 AND active_server_id = $2`,
    [accountId, serverId]
  );
  res.json({ released: result.rowCount === 1 });
});

app.listen(PORT, () => console.log(`Grand City Account Service listening on ${PORT}`));
