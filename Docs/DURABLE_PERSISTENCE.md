# Grand City Mobile Durable Persistence

## Production direction

Player profiles are now designed around a shared PostgreSQL database instead of an Unreal `UGameInstanceSubsystem` memory map.

The repository contains:

- `Backend/db/init.sql` — durable PostgreSQL schema.
- `Backend/src/server.js` — authenticated profile REST API.
- `Backend/docker-compose.yml` — PostgreSQL + API deployment for development/staging.
- `GrandCityDurablePersistenceSubsystem` — Unreal HTTP client.
- Profile loading before player spawn.
- Profile creation persisted immediately for new accounts.
- Automatic saves every 60 seconds and on logout.

## Data that survives a server restart

The database stores character ID/name, level, cash, bank balance, reputation, region, play time, schema version and last-save time.

The PostgreSQL volume must be backed up and retained by the hosting provider. Do not treat the local Docker volume as a disaster-recovery system by itself.

## Cross-server transfer

All game servers must point to the same authoritative database/API. A player should be disconnected from the source server only after the latest profile save succeeds. The destination server then loads the same account profile.

Before production cross-region transfers, add an account/session service with signed authentication tokens and a short-lived transfer lock/lease so two servers cannot mutate the same account simultaneously. The current API already uses an authenticated server-to-database boundary; it is not a replacement for player authentication.

## Security requirements

- Never expose the PostgreSQL port to the public internet.
- Keep `GRANDCITY_INTERNAL_API_KEY` secret and inject it through deployment secrets.
- Use HTTPS/TLS between game servers and the persistence API in production.
- Replace the development `?AccountId=` connection option with a signed account-authentication flow before public release.
- Back up PostgreSQL and test restoration.
- Add audit/event logging for money and inventory mutations before the economy becomes live.

## Local development

From `Backend/`:

```bash
docker compose up --build
```

The API health endpoint is `/health`. The Unreal development config points to `http://127.0.0.1:8080` and contains a placeholder API key; replace it locally with the same secret configured for the API.
