import express from 'express';

const app = express();
app.use(express.json({ limit: '32kb' }));

const PORT = Number(process.env.PORT || 8090);
const INTERNAL_API_KEY = process.env.INTERNAL_API_KEY || '';
const HEARTBEAT_TIMEOUT_MS = Number(process.env.HEARTBEAT_TIMEOUT_MS || 15000);

const regions = new Map([
  ['AFRICA_WEST', { regionId:'AFRICA_WEST', continent:'Africa', endpoint:'gc-africa-west', healthy:false, state:'OFFLINE', players:0, capacity:100, latencyHintMs:60 }],
  ['AFRICA_SOUTH', { regionId:'AFRICA_SOUTH', continent:'Africa', endpoint:'gc-africa-south', healthy:false, state:'OFFLINE', players:0, capacity:100, latencyHintMs:110 }],
  ['AFRICA_EAST', { regionId:'AFRICA_EAST', continent:'Africa', endpoint:'gc-africa-east', healthy:false, state:'OFFLINE', players:0, capacity:100, latencyHintMs:100 }],
  ['EUROPE_WEST', { regionId:'EUROPE_WEST', continent:'Europe', endpoint:'gc-europe-west', healthy:false, state:'OFFLINE', players:0, capacity:100, latencyHintMs:120 }],
  ['EUROPE_CENTRAL', { regionId:'EUROPE_CENTRAL', continent:'Europe', endpoint:'gc-europe-central', healthy:false, state:'OFFLINE', players:0, capacity:100, latencyHintMs:135 }],
  ['EUROPE_NORTH', { regionId:'EUROPE_NORTH', continent:'Europe', endpoint:'gc-europe-north', healthy:false, state:'OFFLINE', players:0, capacity:100, latencyHintMs:150 }],
  ['ASIA_SOUTHEAST', { regionId:'ASIA_SOUTHEAST', continent:'Asia', endpoint:'gc-asia-southeast', healthy:false, state:'OFFLINE', players:0, capacity:100, latencyHintMs:180 }],
  ['ASIA_EAST', { regionId:'ASIA_EAST', continent:'Asia', endpoint:'gc-asia-east', healthy:false, state:'OFFLINE', players:0, capacity:100, latencyHintMs:200 }],
  ['ASIA_SOUTH', { regionId:'ASIA_SOUTH', continent:'Asia', endpoint:'gc-asia-south', healthy:false, state:'OFFLINE', players:0, capacity:100, latencyHintMs:220 }],
  ['NORTH_AMERICA_EAST', { regionId:'NORTH_AMERICA_EAST', continent:'North America', endpoint:'gc-na-east', healthy:false, state:'OFFLINE', players:0, capacity:100, latencyHintMs:210 }],
  ['NORTH_AMERICA_WEST', { regionId:'NORTH_AMERICA_WEST', continent:'North America', endpoint:'gc-na-west', healthy:false, state:'OFFLINE', players:0, capacity:100, latencyHintMs:250 }],
  ['SOUTH_AMERICA_EAST', { regionId:'SOUTH_AMERICA_EAST', continent:'South America', endpoint:'gc-sa-east', healthy:false, state:'OFFLINE', players:0, capacity:100, latencyHintMs:180 }],
  ['SOUTH_AMERICA_WEST', { regionId:'SOUTH_AMERICA_WEST', continent:'South America', endpoint:'gc-sa-west', healthy:false, state:'OFFLINE', players:0, capacity:100, latencyHintMs:220 }],
  ['MIDDLE_EAST', { regionId:'MIDDLE_EAST', continent:'Middle East', endpoint:'gc-middle-east', healthy:false, state:'OFFLINE', players:0, capacity:100, latencyHintMs:170 }],
  ['OCEANIA', { regionId:'OCEANIA', continent:'Oceania', endpoint:'gc-oceania', healthy:false, state:'OFFLINE', players:0, capacity:100, latencyHintMs:240 }]
]);

function authorized(req) {
  return !INTERNAL_API_KEY || req.get('x-internal-api-key') === INTERNAL_API_KEY;
}

function refreshHealth() {
  const now = Date.now();
  for (const server of regions.values()) {
    const alive = Boolean(server.lastHeartbeatAt && now - server.lastHeartbeatAt <= HEARTBEAT_TIMEOUT_MS);
    server.healthy = alive && server.state === 'READY';
    if (!alive) server.state = 'OFFLINE';
  }
}

function score(server, client) {
  const measured = Number(client?.latencyMsByRegion?.[server.regionId]);
  const latency = Number.isFinite(measured) && measured >= 0 ? measured : server.latencyHintMs;
  const utilization = server.capacity > 0 ? server.players / server.capacity : 1;
  return latency + utilization * 250;
}

app.get('/health', (_req, res) => res.json({ ok:true, service:'global-master-router', regions:regions.size }));

app.get('/v1/regions', (_req, res) => {
  refreshHealth();
  res.json({ regions:[...regions.values()].map(({lastHeartbeatAt, ...publicServer}) => publicServer) });
});

app.post('/v1/servers/heartbeat', (req, res) => {
  if (!authorized(req)) return res.status(401).json({ error:'unauthorized' });
  const { regionId, serverId, endpoint, players, capacity, state } = req.body || {};
  const server = regions.get(regionId);
  if (!server || !serverId) return res.status(400).json({ error:'invalid_region' });
  if (state && !['STARTING','READY','DRAINING'].includes(state)) return res.status(400).json({ error:'invalid_state' });
  server.serverId = serverId;
  if (endpoint) server.endpoint = endpoint;
  if (Number.isFinite(players)) server.players = Math.max(0, players);
  if (Number.isFinite(capacity) && capacity > 0) server.capacity = capacity;
  server.state = state || 'READY';
  server.lastHeartbeatAt = Date.now();
  server.healthy = server.state === 'READY';
  res.json({ ok:true, regionId, serverId, state:server.state });
});

app.post('/v1/servers/drain', (req, res) => {
  if (!authorized(req)) return res.status(401).json({ error:'unauthorized' });
  const { regionId, serverId } = req.body || {};
  const server = regions.get(regionId);
  if (!server || server.serverId !== serverId) return res.status(404).json({ error:'server_not_registered' });
  server.state = 'DRAINING';
  server.healthy = false;
  res.json({ ok:true, regionId, serverId, state:'DRAINING' });
});

app.post('/v1/route', (req, res) => {
  refreshHealth();
  const client = req.body || {};
  const candidates = [...regions.values()]
    .filter(server => server.healthy && server.state === 'READY' && server.players < server.capacity)
    .sort((a,b) => score(a, client) - score(b, client));
  if (!candidates.length) return res.status(503).json({ error:'no_healthy_servers' });
  const selected = candidates[0];
  res.json({
    regionId:selected.regionId,
    serverId:selected.serverId || `${selected.regionId}-01`,
    endpoint:selected.endpoint,
    reason:'lowest latency/load score',
    score:score(selected, client)
  });
});

app.listen(PORT, () => console.log(`Grand City global master router listening on ${PORT}`));
