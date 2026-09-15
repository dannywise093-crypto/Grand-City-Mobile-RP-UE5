# Grand City Mobile NPC AI Brain

The NPC brain is a server-side orchestration layer built on top of the Grand City AI Manager.

## Current capabilities

- NPC identity, name, personality, occupation and district.
- Per-player bounded conversation memory.
- Configurable maximum conversation turns (default 8).
- Per-NPC/player dialogue cooldown (default 3 seconds).
- One pending request per NPC/player conversation.
- Input/output length limits and newline sanitization.
- Server-authoritative execution; clients cannot directly invoke the AI transport.
- AI output is dialogue only and cannot directly grant money, inventory, ranks, permissions or purchases.

## Runtime flow

```text
Player message
  -> NPC Brain (server)
  -> bounded context + personality
  -> Grand City AI Manager
  -> AI Gateway
  -> OpenAI provider
  -> validated text response
  -> NPC Brain memory
  -> gameplay/UI presentation
```

## Security boundary

The mobile package must not contain `AI_GATEWAY_KEY` or `OPENAI_API_KEY`. The Unreal gateway client reads `GRANDCITY_AI_GATEWAY_KEY` from the dedicated server environment, with a command-line fallback for controlled server deployments. The AI gateway keeps the provider key server-side.

## Important gameplay rule

AI is advisory. Economy, inventory, purchases, permissions, mission rewards and other authoritative state must remain deterministic server gameplay systems.

## Next hardening work

- Replace per-process gateway rate limiting with a shared Redis-backed limiter.
- Add authenticated player/session identity to AI requests.
- Add request tracing and usage/cost accounting.
- Add profanity/safety and prompt-injection handling appropriate for the game's rating.
- Add automated backend tests and UE5.6 compile/package validation on a real toolchain.
