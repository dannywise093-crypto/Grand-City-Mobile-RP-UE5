# Grand City Mobile — Player Persistence Design

## Purpose

This layer defines the server-authoritative profile contract for player data and connects it to a durable PostgreSQL-backed service.

## Profile data

Each profile contains:

- SchemaVersion
- AccountId
- CharacterId
- CharacterName
- RegionId
- CharacterLevel
- Cash
- BankBalance
- Reputation
- TotalPlayTimeSeconds
- LastSaveUnixSeconds

## Authority rules

- Clients never write persistent cash, bank, level, or reputation directly.
- The authoritative game server loads a profile before allowing the player to spawn.
- New profiles are persisted immediately after creation.
- The authoritative game server saves the profile during logout and controlled checkpoints.
- AccountId is the primary profile key.
- SchemaVersion allows future migrations without changing PlayerState contracts.

## Durable storage

`UGrandCityDurablePersistenceSubsystem` communicates with the Grand City persistence API over HTTP. The API stores profiles in PostgreSQL. The database is external to the Unreal server process, so an Unreal server restart does not erase player data.

The old in-memory persistence subsystem has been removed.

## Server transfers

All regional game servers can use the same authoritative persistence API. A destination server can load the same AccountId after a source server saves the latest profile. Production transfer flow still needs signed player authentication and a short-lived account/session lease to prevent simultaneous writes by two servers.

See `Docs/DURABLE_PERSISTENCE.md` for deployment and security requirements.
