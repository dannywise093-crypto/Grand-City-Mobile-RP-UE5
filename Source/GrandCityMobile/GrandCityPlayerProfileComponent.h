#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrandCityPlayerProfileTypes.h"
#include "GrandCityPlayerProfileComponent.generated.h"

delegate void FGrandCityProfileComponentLoadResult(bool bSuccess);
delegate void FGrandCityProfileComponentSaveResult(bool bSuccess);

UCLASS(ClassGroup=(GrandCity), meta=(BlueprintSpawnableComponent))
class GRANDCITYMOBILE_API UGrandCityPlayerProfileComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGrandCityPlayerProfileComponent();

    void LoadProfile(FGrandCityProfileComponentLoadResult Callback);
    void SaveProfile(FGrandCityProfileComponentSaveResult Callback);

    void ApplyProfileToPlayerState();
    void CaptureProfileFromPlayerState();

    const FGrandCityPlayerProfile& GetProfile() const { return Profile; }

private:
    UPROPERTY()
    FGrandCityPlayerProfile Profile;
};
