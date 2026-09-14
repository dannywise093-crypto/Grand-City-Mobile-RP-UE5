#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GrandCityMobilePlayerController.generated.h"

UCLASS()
class GRANDCITYMOBILE_API AGrandCityMobilePlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AGrandCityMobilePlayerController();

protected:
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

public:
    UFUNCTION(Client, Reliable)
    void ClientInitializeSession();

    UFUNCTION(Client, Reliable)
    void ClientNotifySpawned();

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Session")
    bool bSessionInitialized = false;

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Session")
    bool bSpawnConfirmed = false;
};
