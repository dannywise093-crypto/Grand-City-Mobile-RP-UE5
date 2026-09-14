# Grand City Mobile — Worldwide Infrastructure

Grand City Mobile is designed as a worldwide multiplayer service rather than a single-region game.

## Global topology

```text
Mobile Client
    |
    v
Global Entry / Master Service
    |
    +--> Account/Auth Service
    |
    +--> Region Discovery + Health
    |
    +--> Best-Region Routing
    |
    +--> Transfer Coordinator
    |
    +--> Regional Game Cluster
            |
            +--> Durable PostgreSQL Profile Store
            +--> Redis/session/cache layer (planned)
            +--> Metrics/logging
```

## Initial worldwide regions

- Africa West
- Africa South
- Africa East
- Europe West
- Europe Central
- Europe North
- Asia Southeast
- Asia East
- Asia South
- North America East
- North America West
- South America East
- South America West
- Oceania
- Middle East

These are logical regions. Production deployment can run multiple game-server instances per region, with automatic replacement and scaling behind the regional endpoint.

## Player routing

1. The client authenticates with the account service.
2. The master service evaluates healthy regional servers and latency.
3. A suitable regional server is selected.
4. The client receives a short-lived connection ticket rather than exposing a long-lived account credential in a travel URL.
5. The regional server validates the ticket, claims the account session, loads the durable profile, and spawns the character.

## Server transfer

Transfers must be transactional:

`source save -> transfer lock -> one-time transfer ticket -> target claim -> target profile load -> target spawn -> source release`

The source server must never simply delete a player session before the target has successfully claimed it.

## Persistence

Account identity is global. Character/profile data is durable and can survive regional server restarts and future server transfers. Money and other valuable resources should eventually use an authoritative transaction ledger rather than trusting client-side balances.

## Worldwide reliability requirements

- Multiple game servers per production region
- Health checks and automatic removal of unhealthy servers
- Database backups and point-in-time recovery
- TLS for all production service-to-service and client traffic
- Secret manager for server credentials
- Rate limiting and abuse protection
- Server heartbeat and stale-session recovery
- Observability: metrics, structured logs, traces and alerts
- Capacity-based routing so a healthy region is not selected when saturated
- Graceful server drain before maintenance

## Mobile networking goals

The mobile client should connect to the closest healthy region while preserving a global account identity. Gameplay servers remain authoritative for movement, inventory, money changes and other gameplay state.

The current repository configuration defines worldwide logical endpoints as the first infrastructure layer. DNS/load balancing, master discovery and production deployment manifests are separate infrastructure work and must be added before launch.
