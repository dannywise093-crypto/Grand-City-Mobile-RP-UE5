#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GrandCityPlayerProfileComponent.h"
#include "GrandCityMobilePlayerController.generated.h"

class UGrandCityPlayerProfileComponent;

UCLASS()
class GRANDCITYMOBILE_API AGrandCityMobilePlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AGrandCityMobilePlayerController();

    void LoadPersistentProfile(FGrandCityProfileComponentLoadResult Callback);
    void SavePersistentProfile(FGrandCityProfileComponentSaveResult Callback);

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

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grand City|Persistence", meta=(AllowPrivateAccess="true"))
    TObjectPtr<UGrandCityPlayerProfileComponent> PlayerProfileComponent;
};
