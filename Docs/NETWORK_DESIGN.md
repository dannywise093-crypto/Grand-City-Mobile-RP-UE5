# Grand City Mobile — Multiplayer Network Design

## Objective

Build a server-authoritative RP network that can scale from local development to multiple regional dedicated-server deployments without changing gameplay ownership rules.

## Session flow

```text
Mobile Client
    |
    | 1. Request server list
    v
Master / Match Service
    |
    | 2. Select compatible healthy region
    v
Regional Game Server
    |
    | 3. Authenticate session ticket
    v
PlayerController / PlayerState
    |
    | 4. Spawn authoritative character
    v
Gameplay World
```

## Authority rules

The server is authoritative for gameplay. Unreal's multiplayer architecture is client-server, with the server holding the authoritative game state and clients receiving replicated state. citeturn0search0

### Client may request

- Move.
- Sprint.
- Interact.
- Enter/exit a vehicle.
- Start a job.
- Purchase an item.
- Deposit/withdraw money.
- Change a character cosmetic.

### Server must validate

- Distance to target.
- Ownership.
- Cooldowns.
- Inventory requirements.
- Money requirements.
- Job requirements.
- State transitions.
- Rate limits.
- Session validity.

## Replication categories

### Always relevant or owner relevant

- PlayerState identity information required by the current UI.
- The player's current controlled pawn.
- Current interaction state where needed.

### Spatially relevant

- Nearby players.
- Nearby vehicles.
- NPCs in active areas.
- Interactive world actors.

### Dormant/background

- Static interactive objects that rarely change.
- Far-away gameplay actors where high-frequency updates are unnecessary.

Replication Graph is planned for the scaling layer because it is specifically designed for large numbers of replicated Actors and connections and can organize actors spatially or by gameplay role. citeturn0search2

## RPC policy

Use RPCs for actions that genuinely require an event crossing the network.

Prefer replicated properties/RepNotify for state that needs to remain synchronized. Avoid putting high-frequency input on reliable RPCs without rate limiting. Epic's networking guidance warns that repeatedly triggered reliable RPCs can overflow the reliable RPC queue. citeturn0search0

### Example

```text
Touch Sprint Button
       |
       v
Local Character
       |
       | Server RPC: request sprint state
       v
Authoritative Character
       |
       | Validate stamina/state/cooldown
       v
Replicated SprintState
       |
       +--------> Owner client
       +--------> Nearby clients
```

## Movement

Character movement will use Unreal's networked Character movement foundation rather than inventing a second movement replication system.

The local client may predict its own movement for responsiveness. The server remains authoritative and corrects invalid state.

## Vehicles

Vehicles will follow the same authority model:

```text
Driver Input
   |
   v
Local Vehicle Controller
   |
   v
Server Vehicle
   |
   +--> authoritative transform/state
   +--> ownership/permission validation
   +--> replicated vehicle state
```

Vehicle physics replication will be tested separately from character replication. Unreal provides networked physics facilities for server-authoritative physics actors, so the final implementation will be selected after profiling the vehicle prototype. citeturn0search4

## Spatial replication design

The city is divided into logical replication cells independent from visual World Partition cells.

```text
+-----+-----+-----+-----+-----+
|     |     |     |     |     |
+-----+-----+-----+-----+-----+
|     |  A  |  A  |     |     |
+-----+-----+-----+-----+-----+
|     |  A  |  P  |  B  |     |
+-----+-----+-----+-----+-----+
|     |     |  B  |     |     |
+-----+-----+-----+-----+-----+
|     |     |     |     |     |
+-----+-----+-----+-----+-----+

P = player
A = high-interest cells
B = lower-interest cells
```

The exact cell dimensions will be determined by profiling rather than chosen as a permanent constant now.

## Bandwidth tiers

| Tier | Examples | Target behavior |
|---|---|---|
| 0 | Owned pawn, active vehicle | Highest update priority |
| 1 | Nearby players/vehicles | Frequent updates |
| 2 | Neighborhood NPCs | Reduced frequency |
| 3 | Far/background actors | Dormant or very low frequency |

## Reconnection

A disconnect must not destroy durable player progress.

The server should:

1. Mark the session disconnected.
2. Preserve the durable profile.
3. Release transient gameplay ownership safely.
4. Apply configured idle/timeout rules.
5. Allow a valid new session to reload the profile.

## Regional architecture

Each regional game server runs the same gameplay build.

```text
                     Master Service
                           |
        +------------------+------------------+
        |                  |                  |
     Africa             Europe              Asia
        |                  |                  |
   Game Server(s)     Game Server(s)     Game Server(s)
        |                  |                  |
        +------------------+------------------+
                           |
                    Persistence API
```

The master service is responsible for discovery and routing metadata, not gameplay authority.

Regional servers are authoritative only for the sessions they host.

## Server health

Each game server will expose internal health information to the master service:

- Build/version.
- Region.
- Current players.
- Maximum players.
- CPU load.
- Memory pressure.
- Tick health.
- Network health.
- Maintenance state.
- Match/session availability.

The client should not directly choose a server by trusting an arbitrary address returned from an untrusted client-side configuration.

## Persistence transactions

Economy-changing operations should use unique transaction identifiers.

Example:

```text
Client: BuyItem(transactionId=ABC123)
        |
        v
Game Server validates purchase
        |
        v
Persistence API
        |
        +--> If ABC123 already applied: return previous result
        |
        +--> Otherwise atomically apply transaction
        |
        v
Game Server updates live state
        |
        v
Client receives result
```

This prevents retrying the same request from creating duplicate purchases or rewards.

## Security boundaries

Never trust these values from the client:

- Currency amount.
- Item quantity.
- Property ownership.
- Job payout.
- Vehicle ownership.
- Cooldown completion.
- Teleport destination.
- Inventory contents.

The client provides requests; the server determines whether requests are legal.

## Network testing plan

The multiplayer test matrix will include:

- Two local clients.
- Multiple simulated clients.
- High latency.
- Packet loss.
- Reconnect during interaction.
- Reconnect while driving.
- Server restart handling.
- Full server capacity tests.
- Regional latency comparison.
- Mobile battery/performance profiling.

## Future implementation order

1. `GameState` + `PlayerState` foundation.
2. Dedicated-session travel/connection flow.
3. Character replication hardening.
4. Vehicle replication.
5. Interaction RPC framework.
6. Replication Graph integration.
7. Spatial interest management.
8. Master server contract.
9. Persistence API contract.
10. Regional server orchestration.
