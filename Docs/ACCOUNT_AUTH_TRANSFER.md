# Grand City Mobile — Account, Authentication & Server Transfer

## Goal

A permanent account identity is the authority for player data. Regional Unreal servers never invent account IDs and never treat a local PlayerId as persistent identity.

## Login flow

1. Mobile client registers or logs into the Account Service.
2. Account Service verifies credentials and returns a signed JWT.
3. The client presents the token when connecting to a regional game server.
4. The regional server validates the token with the Account Service.
5. The server claims an account session using its server ID.
6. The server loads the account's PostgreSQL profile.
7. Only after successful authentication/profile load does the server spawn the character.

## Server transfer

1. Source server asks the Account Service for a transfer lock from source to target.
2. Account Service stores a one-time transfer token in `account_sessions` under a row lock.
3. Source server saves the complete profile and releases its active session.
4. Client connects to the target regional server with the authenticated account token and transfer token.
5. Target server claims the session atomically.
6. Target loads the same PostgreSQL profile and spawns the character.

This prevents the same account from being active on two regional servers at the same time during a controlled transfer.

## Security rules

- Passwords are never stored in plaintext.
- JWTs are signed server-side and verified server-side.
- Internal Account Service endpoints require `x-internal-api-key`.
- Game clients never receive database credentials.
- Money, level, bank balance and reputation remain server-authoritative.
- PostgreSQL constraints reject negative cash/bank values and invalid levels.
- Secrets must be supplied through deployment environment variables, never committed to Git.

## Production requirements before launch

- Put the Account Service behind HTTPS/TLS.
- Store secrets in a real secret manager.
- Add refresh-token/session rotation and rate limiting.
- Add server heartbeats and stale-session recovery.
- Add database backups and point-in-time recovery.
- Add audit/ledger records for money-changing operations.
- Add migrations instead of editing the initial schema in place.
- Add authoritative server-to-server authentication (mTLS or equivalent) for production regional infrastructure.
