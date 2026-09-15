#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GrandCityPlayerProfileTypes.h"
#include "GrandCityDurablePersistenceSubsystem.generated.h"

using FGrandCityProfileLoadResult = TFunction<void(bool bSuccess, bool bFound, const FGrandCityPlayerProfile& Profile)>;
using FGrandCityProfileSaveResult = TFunction<void(bool bSuccess, const FGrandCityPlayerProfile& Profile)>;

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
