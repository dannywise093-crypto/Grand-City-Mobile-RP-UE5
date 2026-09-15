# Grand City AI Gateway

The AI gateway is the server-side boundary between Grand City Mobile and an AI provider.

## Flow

`UE5 dedicated server -> Grand City AI Gateway -> AI provider -> gateway -> UE5`

The Android client must never contain the provider API key or the internal gateway key.

## Environment

- `PORT=8100`
- `AI_GATEWAY_KEY=<long-random-secret>`
- `OPENAI_API_KEY=<provider-secret>`
- `OPENAI_MODEL=<approved-model>`
- `OPENAI_BASE_URL=https://api.openai.com/v1`

## Endpoint

`POST /v1/ai/chat`

Header:

`x-ai-gateway-key: <AI_GATEWAY_KEY>`

Body fields:

- `request_id`
- `system_prompt`
- `user_prompt`
- optional `model`

The gateway returns only the generated text and request ID. It does not grant money, inventory, permissions, ranks, purchases, or other authoritative gameplay state.

## Production requirements

Use TLS, a managed secret store, request rate limits, request tracing, provider timeouts, abuse controls, and server-side authorization tied to the player's authenticated game session. Do not expose the internal gateway endpoint directly to untrusted mobile clients.
