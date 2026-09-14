#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GrandCityPlayerProfileComponent.h"
#include "GrandCityMobilePlayerController.generated.h"

class UGrandCityPlayerProfileComponent;
class UGrandCityMobileAuthWidget;

UCLASS()
class GRANDCITYMOBILE_API AGrandCityMobilePlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AGrandCityMobilePlayerController();

    void LoadPersistentProfile(FGrandCityProfileComponentLoadResult Callback);
    void SavePersistentProfile(FGrandCityProfileComponentSaveResult Callback);
    void SetAuthCredentials(const FString& InAuthToken, const FString& InTransferToken, const FString& InConnectionTicket = FString());
    const FString& GetAuthToken() const { return AuthToken; }
    const FString& GetTransferToken() const { return TransferToken; }
    const FString& GetConnectionTicket() const { return ConnectionTicket; }
    void ClearAuthCredentials();
    void TravelToAuthenticatedRegion(const FString& RegionId);
    void TravelToBestWorldwideServer();

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

    UPROPERTY()
    TObjectPtr<UGrandCityMobileAuthWidget> AuthWidget;

    FString AuthToken;
    FString TransferToken;
    FString ConnectionTicket;
};
