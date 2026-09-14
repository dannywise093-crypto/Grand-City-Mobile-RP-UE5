#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GrandCityPlayerProfileTypes.h"
#include "GrandCityPlayerPersistenceSubsystem.generated.h"

UCLASS()
class GRANDCITYMOBILE_API UGrandCityPlayerPersistenceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    bool LoadProfile(const FString& AccountId, FGrandCityPlayerProfile& OutProfile);
    bool SaveProfile(const FGrandCityPlayerProfile& Profile);

private:
    UPROPERTY()
    TMap<FString, FGrandCityPlayerProfile> Profiles;
};
