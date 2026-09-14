CREATE EXTENSION IF NOT EXISTS pgcrypto;

CREATE TABLE IF NOT EXISTS accounts (
    account_id UUID PRIMARY KEY,
    display_name VARCHAR(24) NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    region_id VARCHAR(64) NOT NULL DEFAULT 'AFRICA_WEST',
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_login_at TIMESTAMPTZ
);

CREATE TABLE IF NOT EXISTS player_profiles (
    account_id UUID PRIMARY KEY REFERENCES accounts(account_id) ON DELETE CASCADE,
    schema_version INTEGER NOT NULL DEFAULT 1,
    character_id VARCHAR(128) NOT NULL UNIQUE,
    character_name VARCHAR(32) NOT NULL,
    region_id VARCHAR(64) NOT NULL DEFAULT 'AFRICA_WEST',
    character_level INTEGER NOT NULL DEFAULT 1 CHECK (character_level >= 1),
    cash BIGINT NOT NULL DEFAULT 0 CHECK (cash >= 0),
    bank_balance BIGINT NOT NULL DEFAULT 0 CHECK (bank_balance >= 0),
    reputation INTEGER NOT NULL DEFAULT 0,
    total_play_time_seconds BIGINT NOT NULL DEFAULT 0 CHECK (total_play_time_seconds >= 0),
    last_save_unix_seconds BIGINT NOT NULL DEFAULT 0,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS account_sessions (
    account_id UUID PRIMARY KEY REFERENCES accounts(account_id) ON DELETE CASCADE,
    active_server_id VARCHAR(128) NOT NULL,
    transfer_token UUID,
    transfer_target_server_id VARCHAR(128),
    transfer_started_at TIMESTAMPTZ,
    last_heartbeat_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_sessions_server ON account_sessions(active_server_id);
CREATE INDEX IF NOT EXISTS idx_profiles_region ON player_profiles(region_id);
