CREATE TABLE IF NOT EXISTS player_profiles (
    account_id TEXT PRIMARY KEY,
    character_id TEXT NOT NULL,
    character_name TEXT NOT NULL,
    region_id TEXT NOT NULL,
    character_level INTEGER NOT NULL DEFAULT 1 CHECK (character_level >= 1),
    cash BIGINT NOT NULL DEFAULT 0 CHECK (cash >= 0),
    bank_balance BIGINT NOT NULL DEFAULT 0 CHECK (bank_balance >= 0),
    reputation INTEGER NOT NULL DEFAULT 0,
    total_play_time_seconds BIGINT NOT NULL DEFAULT 0 CHECK (total_play_time_seconds >= 0),
    schema_version INTEGER NOT NULL DEFAULT 1,
    version BIGINT NOT NULL DEFAULT 1,
    last_save_unix_seconds BIGINT NOT NULL DEFAULT 0,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS player_profiles_region_idx ON player_profiles(region_id);

CREATE OR REPLACE FUNCTION set_updated_at() RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS player_profiles_updated_at ON player_profiles;
CREATE TRIGGER player_profiles_updated_at
BEFORE UPDATE ON player_profiles
FOR EACH ROW EXECUTE FUNCTION set_updated_at();
