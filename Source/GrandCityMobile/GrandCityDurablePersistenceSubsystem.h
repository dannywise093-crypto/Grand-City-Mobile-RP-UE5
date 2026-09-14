#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GrandCityPlayerProfileTypes.h"
#include "GrandCityDurablePersistenceSubsystem.generated.h"

delegate void FGrandCityProfileLoadResult(bool bSuccess, bool bFound, const FGrandCityPlayerProfile& Profile);
delegate void FGrandCityProfileSaveResult(bool bSuccess, const FGrandCityPlayerProfile& Profile);

UCLASS()
class GRANDCITYMOBILE_API UGrandCityDurablePersistenceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    void LoadProfile(const FString& AccountId, FGrandCityProfileLoadResult Callback);
    void SaveProfile(const FGrandCityPlayerProfile& Profile, FGrandCityProfileSaveResult Callback);

private:
    FString GetBaseUrl() const;
    FString GetApiKey() const;
};
