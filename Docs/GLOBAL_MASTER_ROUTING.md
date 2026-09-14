# Grand City Mobile — Global Master Routing

## Goal

Grand City Mobile is designed as a worldwide multiplayer service. The mobile client authenticates once, contacts the global master router, and is directed to a healthy regional server with the best available latency/load score.

## Flow

1. Mobile client authenticates with the Account Service.
2. Client receives an account token.
3. Client asks the Global Master Router for the best server.
4. Router considers regional health, capacity and client latency measurements when supplied.
5. Router returns region, server ID and endpoint.
6. Client travels to the selected dedicated server.
7. Regional server verifies the account token and claims the account session.
8. PostgreSQL profile persistence loads the same character, level, money and other durable state.

## Global regions

Africa West/South/East, Europe West/Central/North, Asia Southeast/East/South, North America East/West, South America East/West, Middle East and Oceania are represented in the registry. Each logical region can later contain multiple physical server instances.

## Health

Regional dedicated servers should heartbeat the master router using the server-only API key. A region is considered unhealthy after the heartbeat timeout and is removed from normal routing.

## Scaling

The registry is intentionally stateless in the prototype. Production deployment should put the router behind a load balancer and use a shared service registry or database/Redis so multiple router instances have consistent server health state.

## Security

The mobile application must never contain the master-router internal API key or regional server credentials. The current connection URL still carries the account JWT as a prototype. Production should replace that with a short-lived, one-time connection ticket issued by the backend.

## Production requirements

- Anycast/GeoDNS or global edge routing to the master service.
- Multiple master-router instances per major cloud region.
- TLS everywhere.
- Short-lived connection tickets instead of JWTs in travel URLs.
- DDoS protection and rate limiting.
- Regional capacity reservations and queueing when full.
- Automated server registration/draining.
- Metrics for latency, packet loss, CPU, memory and player capacity.
- Database backups/PITR and durable profile revisions.
