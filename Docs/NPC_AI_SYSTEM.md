# Grand City Mobile NPC AI System

## Runtime flow

Player approaches NPC -> mobile interaction check -> NPC dialogue request -> server-side NPC Brain -> Grand City AI Manager -> AI Gateway -> approved AI provider -> validated response -> NPC dialogue UI.

## Components

- `UGrandCityNPCInteractionComponent`: distance and interaction-state bridge for mobile UI.
- `UGrandCityNPCBrain`: NPC identity, personality, occupation, district, bounded per-player memory and cooldowns.
- `UGrandCityAIManager`: server-safe AI orchestration and request tracking.
- `UGrandCityAIGatewayClient`: HTTP transport to the server-side AI gateway.

## Authority boundary

AI output is narrative text only. The AI layer must not directly modify cash, bank balance, inventory, vehicles, permissions, purchases, ranks, or authentication state. Gameplay systems validate and apply any authoritative result.

## Mobile UX

The intended mobile interaction is a lightweight context action such as `Talk` when the player is within interaction distance. The UI should avoid polling the AI continuously. Only send a request after explicit player interaction.

## Performance

NPC requests are asynchronous, rate-limited per NPC/player pair, and bounded to a small conversation history. Do not run AI inference in Tick. Keep NPC AI optional and fall back to deterministic dialogue when the gateway is unavailable.

## Production requirements

Use TLS, authenticated server-to-gateway traffic, managed secrets, gateway rate limiting, request tracing, provider timeouts, abuse controls and server-side logging with sensitive player data minimized.
