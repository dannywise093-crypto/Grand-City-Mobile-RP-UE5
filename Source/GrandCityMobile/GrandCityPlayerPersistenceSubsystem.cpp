#include "GrandCityPlayerPersistenceSubsystem.h"

bool UGrandCityPlayerPersistenceSubsystem::LoadProfile(const FString& AccountId, FGrandCityPlayerProfile& OutProfile)
{
    if (AccountId.IsEmpty())
    {
        return false;
    }

    if (const FGrandCityPlayerProfile* Existing = Profiles.Find(AccountId))
    {
        OutProfile = *Existing;
        return true;
    }

    OutProfile = FGrandCityPlayerProfile();
    OutProfile.AccountId = AccountId;
    OutProfile.CharacterId = FString::Printf(TEXT("CHAR-%s"), *AccountId);
    OutProfile.CharacterName = AccountId;
    return false;
}

bool UGrandCityPlayerPersistenceSubsystem::SaveProfile(const FGrandCityPlayerProfile& Profile)
{
    if (Profile.AccountId.IsEmpty())
    {
        return false;
    }

    Profiles.Add(Profile.AccountId, Profile);
    return true;
}
