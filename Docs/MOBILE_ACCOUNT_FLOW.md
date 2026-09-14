# Grand City Mobile — Mobile Account Flow

The mobile client uses only the public account endpoints:

- `POST /v1/accounts/register`
- `POST /v1/accounts/login`

The client receives an account UUID, display name, region and short-lived/renewable authentication credential from the Account Service. The mobile app must never contain the server-only `INTERNAL_API_KEY` or PostgreSQL credentials.

## Runtime flow

1. Player opens Grand City Mobile.
2. Login/Register UI calls the Account Service over HTTPS.
3. Account Service validates credentials and returns the account identity and token.
4. The client selects/connects to the appropriate regional game server.
5. The regional dedicated server validates the token using its server-only credential.
6. The server claims the account session.
7. The server loads the durable PostgreSQL profile.
8. Character spawning is allowed only after authentication, session claim and profile load succeed.

## Security

Development may use localhost. Production must use HTTPS/TLS. Never ship `INTERNAL_API_KEY`, database credentials or JWT signing secrets in the Android package.
