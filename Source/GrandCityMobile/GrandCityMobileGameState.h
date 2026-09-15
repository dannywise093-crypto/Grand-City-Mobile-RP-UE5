#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GrandCityMobileGameState.generated.h"

UCLASS()
class GRANDCITYMOBILE_API AGrandCityMobileGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    AGrandCityMobileGameState();

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Grand City|World")
    int32 OnlinePlayerCount = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Grand City|World")
    int32 ServerPopulationLimit = 100;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Grand City|World")
    FName RegionId = TEXT("AFRICA_WEST");

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Grand City|World")
    FName ServerId = TEXT("GC-AFRICA-01");

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
