import express from 'express';

const app = express();
app.use(express.json({ limit: '256kb' }));

const PORT = Number(process.env.PORT || 8100);
const AI_GATEWAY_KEY = process.env.AI_GATEWAY_KEY || '';
const OPENAI_API_KEY = process.env.OPENAI_API_KEY || '';
const OPENAI_BASE_URL = process.env.OPENAI_BASE_URL || 'https://api.openai.com/v1';
const DEFAULT_MODEL = process.env.OPENAI_MODEL || 'gpt-5.6-mini';

function authorized(req) {
  return Boolean(AI_GATEWAY_KEY) && req.get('x-ai-gateway-key') === AI_GATEWAY_KEY;
}

function validateInput(body) {
  if (!body || typeof body !== 'object') return 'Invalid request body.';
  if (typeof body.system_prompt !== 'string' || body.system_prompt.length > 8000) return 'Invalid system prompt.';
  if (typeof body.user_prompt !== 'string' || body.user_prompt.length > 8000) return 'Invalid user prompt.';
  if (body.request_id && String(body.request_id).length > 128) return 'Invalid request ID.';
  return null;
}

app.get('/health', (_req, res) => {
  res.json({ ok: true, service: 'grand-city-ai-gateway', provider_configured: Boolean(OPENAI_API_KEY) });
});

app.post('/v1/ai/chat', async (req, res) => {
  if (!authorized(req)) return res.status(401).json({ error: 'Unauthorized.' });

  const validationError = validateInput(req.body);
  if (validationError) return res.status(400).json({ error: validationError });
  if (!OPENAI_API_KEY) return res.status(503).json({ error: 'AI provider is not configured.' });

  const model = typeof req.body.model === 'string' && req.body.model.length <= 100 ? req.body.model : DEFAULT_MODEL;

  try {
    const response = await fetch(`${OPENAI_BASE_URL}/chat/completions`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        Authorization: `Bearer ${OPENAI_API_KEY}`
      },
      body: JSON.stringify({
        model,
        messages: [
          { role: 'system', content: req.body.system_prompt },
          { role: 'user', content: req.body.user_prompt }
        ],
        temperature: 0.7,
        max_tokens: 500
      })
    });

    const data = await response.json();
    if (!response.ok) {
      return res.status(502).json({ error: 'AI provider request failed.', provider_status: response.status });
    }

    const text = data?.choices?.[0]?.message?.content;
    if (typeof text !== 'string') return res.status(502).json({ error: 'AI provider returned no text.' });

    return res.json({ request_id: req.body.request_id || '', text });
  } catch (_error) {
    return res.status(502).json({ error: 'AI gateway provider connection failed.' });
  }
});

app.listen(PORT, () => console.log(`Grand City AI Gateway listening on ${PORT}`));
