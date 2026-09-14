import express from 'express';
import { createClient } from 'redis';

const app = express();
app.use(express.json({ limit: '32kb' }));

const PORT = Number(process.env.PORT || 8090);
const INTERNAL_API_KEY = process.env.INTERNAL_API_KEY || '';
const HEARTBEAT_TIMEOUT_MS = Number(process.env.HEARTBEAT_TIMEOUT_MS || 15000);
const REDIS_URL = process.env.REDIS_URL || 'redis://127.0.0.1:6379';
const REGISTRY_KEY = process.env.REGISTRY_KEY || 'grandcity:router:regions';

const DEFAULT_REGIONS = [
  ['AFRICA_WEST','Africa','gc-africa-west',60], ['AFRICA_SOUTH','Africa','gc-africa-south',110], ['AFRICA_EAST','Africa','gc-africa-east',100],
  ['EUROPE_WEST','Europe','gc-europe-west',120], ['EUROPE_CENTRAL','Europe','gc-europe-central',135], ['EUROPE_NORTH','Europe','gc-europe-north',150],
  ['ASIA_SOUTHEAST','Asia','gc-asia-southeast',180], ['ASIA_EAST','Asia','gc-asia-east',200], ['ASIA_SOUTH','Asia','gc-asia-south',220],
  ['NORTH_AMERICA_EAST','North America','gc-na-east',210], ['NORTH_AMERICA_WEST','North America','gc-na-west',250],
  ['SOUTH_AMERICA_EAST','South America','gc-sa-east',180], ['SOUTH_AMERICA_WEST','South America','gc-sa-west',220],
  ['MIDDLE_EAST','Middle East','gc-middle-east',170], ['OCEANIA','Oceania','gc-oceania',240]
];

const redis = createClient({ url: REDIS_URL });
redis.on('error', error => console.error('Redis error:', error));

function authorized(req) {
  return Boolean(INTERNAL_API_KEY) && req.get('x-internal-api-key') === INTERNAL_API_KEY;
}

function defaults() {
  return DEFAULT_REGIONS.map(([regionId, continent, endpoint, latencyHintMs]) => ({
    regionId, continent, endpoint, healthy:false, state:'OFFLINE', players:0, capacity:100, latencyHintMs
  }));
}

async function readRegistry() {
  const raw = await redis.hGetAll(REGISTRY_KEY);
  const byRegion = new Map();
  for (const [regionId, value] of Object.entries(raw)) {
    try { byRegion.set(regionId, JSON.parse(value)); } catch { /* ignore corrupt entry */ }
  }
  return defaults().map(server => ({ ...server, ...(byRegion.get(server.regionId) || {}) }));
}

async function writeServer(server) {
  await redis.hSet(REGISTRY_KEY, server.regionId, JSON.stringify(server));
}

function refreshHealth(servers) {
  const now = Date.now();
  return servers.map(server => {
    const alive = Boolean(server.lastHeartbeatAt && now - Number(server.lastHeartbeatAt) <= HEARTBEAT_TIMEOUT_MS);
    return { ...server, healthy: alive && server.state === 'READY', state: alive ? server.state : 'OFFLINE' };
  });
}

function score(server, client) {
  const measured = Number(client?.latencyMsByRegion?.[server.regionId]);
  const latency = Number.isFinite(measured) && measured >= 0 ? measured : Number(server.latencyHintMs);
  const utilization = Number(server.capacity) > 0 ? Number(server.players) / Number(server.capacity) : 1;
  return latency + utilization * 250;
}

app.get('/health', async (_req, res) => {
  try { await redis.ping(); res.json({ ok:true, service:'global-master-router', regions:DEFAULT_REGIONS.length, registry:'redis' }); }
  catch { res.status(503).json({ ok:false, registry:'unavailable' }); }
});

app.get('/v1/regions', async (_req, res) => {
  try {
    const servers = refreshHealth(await readRegistry());
    for (const server of servers) await writeServer(server);
    res.json({ regions:servers });
  } catch (error) { console.error(error); res.status(503).json({ error:'registry_unavailable' }); }
});

app.post('/v1/servers/heartbeat', async (req, res) => {
  if (!authorized(req)) return res.status(401).json({ error:'unauthorized' });
  const { regionId, serverId, endpoint, players, capacity, state } = req.body || {};
  if (!regionId || !serverId || !DEFAULT_REGIONS.some(item => item[0] === regionId)) return res.status(400).json({ error:'invalid_region' });
  if (state && !['STARTING','READY','DRAINING'].includes(state)) return res.status(400).json({ error:'invalid_state' });
  try {
    const current = (await readRegistry()).find(server => server.regionId === regionId) || defaults().find(server => server.regionId === regionId);
    const updated = {
      ...current, serverId,
      endpoint: endpoint || current.endpoint,
      players: Number.isFinite(players) ? Math.max(0, players) : current.players,
      capacity: Number.isFinite(capacity) && capacity > 0 ? capacity : current.capacity,
      state: state || 'READY', lastHeartbeatAt:Date.now(), healthy:(state || 'READY') === 'READY'
    };
    await writeServer(updated);
    res.json({ ok:true, regionId, serverId, state:updated.state });
  } catch (error) { console.error(error); res.status(503).json({ error:'registry_unavailable' }); }
});

app.post('/v1/servers/drain', async (req, res) => {
  if (!authorized(req)) return res.status(401).json({ error:'unauthorized' });
  const { regionId, serverId } = req.body || {};
  try {
    const server = (await readRegistry()).find(item => item.regionId === regionId && item.serverId === serverId);
    if (!server) return res.status(404).json({ error:'server_not_registered' });
    const updated = { ...server, state:'DRAINING', healthy:false, lastHeartbeatAt:Date.now() };
    await writeServer(updated);
    res.json({ ok:true, regionId, serverId, state:'DRAINING' });
  } catch (error) { console.error(error); res.status(503).json({ error:'registry_unavailable' }); }
});

app.post('/v1/route', async (req, res) => {
  try {
    const servers = refreshHealth(await readRegistry());
    for (const server of servers) await writeServer(server);
    const candidates = servers.filter(server => server.healthy && server.state === 'READY' && Number(server.players) < Number(server.capacity)).sort((a,b) => score(a, req.body || {}) - score(b, req.body || {}));
    if (!candidates.length) return res.status(503).json({ error:'no_healthy_servers' });
    const selected = candidates[0];
    res.json({ regionId:selected.regionId, serverId:selected.serverId || `${selected.regionId}-01`, endpoint:selected.endpoint, reason:'lowest latency/load score', score:score(selected, req.body || {}) });
  } catch (error) { console.error(error); res.status(503).json({ error:'registry_unavailable' }); }
});

async function start() {
  await redis.connect();
  app.listen(PORT, () => console.log(`Grand City global master router listening on ${PORT} with Redis registry`));
}

start().catch(error => { console.error('Failed to start master router:', error); process.exit(1); });
