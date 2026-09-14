# Grand City Mobile — Technical Architecture

## 1. Product target

Grand City Mobile is a mobile-first Unreal Engine 5 multiplayer roleplay game with an original world, original characters, original vehicles, original UI, and original gameplay systems.

The architecture is designed around four priorities:

1. Server-authoritative multiplayer.
2. Large-city streaming without loading the whole world on a phone.
3. Mobile performance as a first-class requirement.
4. Systems that can grow from prototype to regional live servers without a rewrite.

## 2. High-level architecture

```text
                         GRAND CITY BACKEND

        +----------------------+      +----------------------+
        | Account/Auth Service |----->| Master/Match Service |
        +----------------------+      +----------+-----------+
                                                |
                           +--------------------+--------------------+
                           |                    |                    |
                    +------v------+      +------v------+      +------v------+
                    | Africa Game |      | Europe Game |      | Asia Game   |
                    |   Server    |      |   Server    |      |   Server    |
                    +------+------+      +------+------+      +------+------+ 
                           |                    |                    |
                           +--------------------+--------------------+
                                                |
                                         Persistent Data
                                         / Economy / RP

                         UNREAL ENGINE CLIENT
                                  |
                +-----------------+------------------+
                |                 |                  |
          Presentation       Local Prediction    Network Layer
          / UI / Audio       / Input / Camera    / Replication
                |                 |                  |
                +-----------------+------------------+
                                  |
                         Gameplay Framework
                                  |
        Character / Vehicle / NPC / Jobs / Inventory / Property
```

## 3. Unreal runtime authority model

The dedicated game server is authoritative for gameplay state. Clients send intent/input and requests; the server validates and changes authoritative state; relevant results are replicated back to clients.

### Server-owned systems

- Player identity inside a game session.
- Character position and authoritative movement state.
- Vehicle ownership and authoritative vehicle state.
- Money and economy transactions.
- Inventory changes.
- Property ownership.
- Job and mission progression.
- NPC state that affects gameplay.
- World events.
- Anti-cheat validation.
- Persistent save requests.

### Client-owned presentation

- Local camera.
- Touch UI.
- HUD presentation.
- Local animation presentation.
- Audio playback.
- Visual effects.
- Non-authoritative prediction/interpolation.

Clients must never be trusted to directly decide persistent economy or progression values.

## 4. Unreal class responsibilities

### GameInstance

Client/server process lifetime state that does not belong to one map or one player.

Examples:
- Connection/session bootstrap.
- Backend endpoint configuration.
- Client service managers.

### GameMode

Server-only rules for the current game world.

Examples:
- Login/spawn rules.
- Match/session rules.
- Respawn rules.
- Server-side gameplay validation.

### GameState

Replicated state shared by players in the current world.

Examples:
- Current server time.
- World event state.
- Server population summary.
- Current city event identifiers.

### PlayerController

Per-player network connection and input-facing orchestration.

### PlayerState

Replicated identity/state belonging to a player.

Examples:
- Player ID.
- Display name.
- Character slot.
- Job role summary.
- Public RP status.

### Character

The player's physical avatar and movement representation.

### Actor Components

Reusable gameplay capabilities rather than putting every feature into the Character class.

Planned components:
- `GCInventoryComponent`
- `GCMoneyComponent`
- `GCInteractionComponent`
- `GCJobComponent`
- `GCPropertyComponent`
- `GCVehicleInteractionComponent`
- `GCStatusComponent`

## 5. World architecture

The final city should use Unreal World Partition rather than one giant always-loaded level. World Partition divides a persistent world into streamable grid cells and loads cells around streaming sources. citeturn1search1

Planned layers:

```text
GrandCityWorld
├── Persistent world configuration
├── Roads and major landmarks
├── District cells
├── Buildings
├── Interior/interaction spaces
├── NPC spawn regions
├── Traffic regions
├── Gameplay event Data Layers
└── HLOD representation
```

HLOD will represent distant, non-interactive city content so the mobile client does not need every distant mesh loaded individually. Epic documents HLOD as a way to reduce draw calls and represent unloaded World Partition cells with proxy content. citeturn1search0

Runtime Data Layers will be reserved for systems such as events, temporary world states, and controlled gameplay variants. citeturn1search7

## 6. City generation strategy

The prototype procedural city system can generate deterministic roads, blocks, and placeholder buildings. It is a development tool, not the final art pipeline.

Generation must be deterministic from:

```text
WorldSeed + DistrictId + BlockId
```

This allows the server and client to agree on structural information without transmitting every static building transform over the network.

Final authored districts can replace procedural placeholders while preserving the same district/grid coordinates.

## 7. Multiplayer relevancy

Grand City should not replicate every actor to every player.

The planned replication strategy is spatial:

```text
                 CITY REPLICATION GRID

       +---------+---------+---------+
       | Cell A  | Cell B  | Cell C  |
       +---------+---------+---------+
       | Cell D  | PLAYER  | Cell F  |
       +---------+---------+---------+
       | Cell G  | Cell H  | Cell I  |
       +---------+---------+---------+

Player receives high-frequency state nearby.
Far-away actors receive reduced or no updates when appropriate.
```

Replication Graph is the planned scaling path for large numbers of replicated actors because it can maintain persistent actor lists and produce relevant replication lists for each connection more efficiently than making every actor evaluate every connection independently. citeturn0search2

## 8. Network update tiers

### Tier 0 — owner/high priority

Examples:
- Local player's authoritative state.
- Current vehicle.
- Immediate interaction target.

### Tier 1 — nearby gameplay

Examples:
- Nearby players.
- Nearby vehicles.
- Active NPCs.
- Nearby interactive objects.

### Tier 2 — neighborhood

Examples:
- Distant players.
- Traffic outside immediate interaction range.
- Low-priority NPCs.

### Tier 3 — background

Examples:
- Static world state.
- Dormant actors.
- Distant cosmetic activity.

Dormancy, relevancy, and priority will be used deliberately to control bandwidth. Epic's networking guidance specifically highlights these tools for managing replication cost. citeturn0search0

## 9. Mobile performance budget

The mobile renderer is treated as the baseline, not a later optimization pass.

Core rules:

- Avoid unnecessary per-frame Tick functions.
- Prefer instancing for repeated static objects.
- Use aggressive but visually acceptable LODs.
- Use HLOD for distant city content.
- Keep transparent materials limited.
- Avoid expensive post-processing by default.
- Profile on real Android devices early.
- Create scalability tiers for low, medium, high, and ultra devices.

Epic's mobile guidance notes that expensive post-processing can be particularly costly on mobile and recommends device-specific scalability and profiling. citeturn1search2turn1search3

## 10. Server regions

Initial logical regions:

- Africa
- Europe
- Middle East
- Asia
- North America
- South America
- Oceania

The master service will eventually return an appropriate region/server based on availability, measured latency, version compatibility, and maintenance state.

A region is a deployment boundary, not a separate gameplay codebase. All regions run the same server build and data schema, with configuration determining capacity and regional settings.

## 11. Persistent data boundary

The game server owns live gameplay state. Persistent backend services own durable player data.

```text
Game Server
    |
    +-- Character save
    +-- Money transaction
    +-- Inventory transaction
    +-- Property transaction
    +-- Job progression
    |
    v
Persistence API
    |
    +-- Player profile DB
    +-- Economy DB
    +-- Property DB
    +-- Audit/event records
```

Important economy operations must be transaction-oriented and idempotent so reconnects or retries do not accidentally duplicate rewards or payments.

## 12. Security model

Never trust client-provided values for:

- Money balance.
- Item creation.
- Item quantity.
- Property ownership.
- Job rewards.
- Vehicle ownership.
- Teleport destination.
- Server-side cooldown completion.

The client can request an action. The server validates whether the action is legal.

## 13. Development phases

### Phase 1 — Core foundation

- Character.
- Replication.
- GameMode/GameState/PlayerState.
- Basic session connection.
- Mobile input.

### Phase 2 — World foundation

- World Partition map.
- District coordinates.
- Roads.
- Procedural prototype blocks.
- Streaming/HLOD strategy.

### Phase 3 — Vehicles

- Vehicle pawn.
- Server-authoritative driving.
- Enter/exit interaction.
- Vehicle ownership.
- Replicated vehicle state.

### Phase 4 — RP systems

- Interaction framework.
- Inventory.
- Money.
- Jobs.
- Properties.
- Businesses.
- Factions.

### Phase 5 — Live multiplayer

- Dedicated server packaging.
- Master server.
- Regional server discovery.
- Persistence API.
- Monitoring.
- Anti-cheat validation.

### Phase 6 — Mobile optimization

- Device profiles.
- Scalability tiers.
- Texture/mesh budgets.
- HLOD.
- Network bandwidth profiling.
- Android profiling.

## 14. Non-negotiable architecture rules

1. `main` is production and should only receive reviewed changes through pull requests.
2. Gameplay authority lives on the server.
3. Client input is intent, not authority.
4. Persistent economy changes are server validated.
5. Static world content should not be unnecessarily replicated.
6. Large-world streaming is designed from the beginning.
7. Mobile performance is tested continuously.
8. Original assets and original implementations are required.
9. Avoid placing every system into the Character class.
10. Prefer small, testable subsystems with clear ownership boundaries.
