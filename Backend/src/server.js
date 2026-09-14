import express from 'express';
import pg from 'pg';

const { Pool } = pg;
const app = express();
const pool = new Pool({ connectionString: process.env.DATABASE_URL });
const port = Number(process.env.PORT || 8080);
const apiKey = process.env.GRANDCITY_INTERNAL_API_KEY || '';

app.use(express.json({ limit: '64kb' }));

function authorized(req) {
  return Boolean(apiKey) && req.get('authorization') === `Bearer ${apiKey}`;
}

function validAccountId(value) {
  return typeof value === 'string' && /^[A-Za-z0-9._:-]{3,128}$/.test(value);
}

function profileFromRow(row) {
  return {
    schemaVersion: Number(row.schema_version),
    accountId: row.account_id,
    characterId: row.character_id,
    characterName: row.character_name,
    regionId: row.region_id,
    characterLevel: Number(row.character_level),
    cash: Number(row.cash),
    bankBalance: Number(row.bank_balance),
    reputation: Number(row.reputation),
    totalPlayTimeSeconds: Number(row.total_play_time_seconds),
    lastSaveUnixSeconds: Number(row.last_save_unix_seconds),
    version: Number(row.version)
  };
}

app.get('/health', async (_req, res) => {
  try {
    await pool.query('SELECT 1');
    res.json({ ok: true, service: 'grand-city-persistence-api' });
  } catch {
    res.status(503).json({ ok: false });
  }
});

app.use('/v1', (req, res, next) => {
  if (!authorized(req)) return res.status(401).json({ error: 'unauthorized' });
  next();
});

app.get('/v1/profiles/:accountId', async (req, res) => {
  const { accountId } = req.params;
  if (!validAccountId(accountId)) return res.status(400).json({ error: 'invalid_account_id' });
  const result = await pool.query('SELECT * FROM player_profiles WHERE account_id = $1', [accountId]);
  if (result.rowCount === 0) return res.status(404).json({ error: 'profile_not_found' });
  res.json(profileFromRow(result.rows[0]));
});

app.put('/v1/profiles/:accountId', async (req, res) => {
  const { accountId } = req.params;
  const p = req.body || {};
  if (!validAccountId(accountId)) return res.status(400).json({ error: 'invalid_account_id' });
  if (p.accountId !== accountId || typeof p.characterId !== 'string' || typeof p.characterName !== 'string' || typeof p.regionId !== 'string') {
    return res.status(400).json({ error: 'invalid_profile' });
  }

  const client = await pool.connect();
  try {
    await client.query('BEGIN');
    const current = await client.query('SELECT version FROM player_profiles WHERE account_id = $1 FOR UPDATE', [accountId]);
    const expected = p.version == null ? null : Number(p.version);

    if (current.rowCount > 0 && expected !== null && Number(current.rows[0].version) !== expected) {
      await client.query('ROLLBACK');
      return res.status(409).json({ error: 'version_conflict', currentVersion: Number(current.rows[0].version) });
    }

    const result = await client.query(`
      INSERT INTO player_profiles
        (account_id, character_id, character_name, region_id, character_level, cash, bank_balance, reputation, total_play_time_seconds, schema_version, version, last_save_unix_seconds)
      VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,1,$11)
      ON CONFLICT (account_id) DO UPDATE SET
        character_id=EXCLUDED.character_id,
        character_name=EXCLUDED.character_name,
        region_id=EXCLUDED.region_id,
        character_level=EXCLUDED.character_level,
        cash=EXCLUDED.cash,
        bank_balance=EXCLUDED.bank_balance,
        reputation=EXCLUDED.reputation,
        total_play_time_seconds=EXCLUDED.total_play_time_seconds,
        schema_version=EXCLUDED.schema_version,
        version=player_profiles.version+1,
        last_save_unix_seconds=EXCLUDED.last_save_unix_seconds
      RETURNING *`, [
        accountId,
        p.characterId,
        p.characterName,
        p.regionId,
        Math.max(1, Number(p.characterLevel || 1)),
        Math.max(0, Number(p.cash || 0)),
        Math.max(0, Number(p.bankBalance || 0)),
        Number(p.reputation || 0),
        Math.max(0, Number(p.totalPlayTimeSeconds || 0)),
        Math.max(1, Number(p.schemaVersion || 1)),
        Math.max(0, Number(p.lastSaveUnixSeconds || 0))
      ]);

    await client.query('COMMIT');
    res.json(profileFromRow(result.rows[0]));
  } catch (error) {
    await client.query('ROLLBACK');
    console.error(error);
    res.status(500).json({ error: 'database_error' });
  } finally {
    client.release();
  }
});

app.listen(port, '0.0.0.0', () => console.log(`Grand City persistence API listening on ${port}`));
