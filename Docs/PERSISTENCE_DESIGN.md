# Grand City Mobile — Player Persistence Design

## Purpose

This layer defines the server-authoritative profile contract for player data. It separates gameplay state from the future external database/backend so the storage implementation can evolve without changing PlayerState contracts.

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
- The authoritative game server loads a profile during login and applies it to PlayerState.
- The authoritative game server saves the profile during logout and controlled persistence checkpoints.
- AccountId is the primary profile key.
- SchemaVersion allows future migrations without changing the external contract.

## Storage boundary

`UGrandCityPlayerPersistenceSubsystem` currently provides an in-memory server-side storage adapter. This is intentionally not presented as production persistence: data will be lost when the server process exits.

The subsystem is the boundary for the future database/backend adapter. A later implementation can connect this interface to the Grand City master/account service and a durable database without making clients responsible for persistence.

## Next persistence stages

1. Apply loaded profiles to PlayerState during login.
2. Save profiles during logout and controlled checkpoints.
3. Add profile validation and migration handling.
4. Add durable backend/database storage.
5. Add cross-region/session transfer safeguards.
