#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrandCityPlayerProfileTypes.h"
#include "GrandCityPlayerProfileComponent.generated.h"

UCLASS(ClassGroup=(GrandCity), meta=(BlueprintSpawnableComponent))
class GRANDCITYMOBILE_API UGrandCityPlayerProfileComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGrandCityPlayerProfileComponent();

    bool LoadProfile();
    bool SaveProfile();

    void ApplyProfileToPlayerState();
    void CaptureProfileFromPlayerState();

    const FGrandCityPlayerProfile& GetProfile() const { return Profile; }

private:
    UPROPERTY()
    FGrandCityPlayerProfile Profile;
};
