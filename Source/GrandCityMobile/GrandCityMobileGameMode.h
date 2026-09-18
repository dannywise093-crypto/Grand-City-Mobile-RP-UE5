#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GrandCityMobileGameMode.generated.h"

class AController;
class APlayerController;

UCLASS(Config=Game)
class GRANDCITYMOBILE_API AGrandCityMobileGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AGrandCityMobileGameMode();

protected:
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void RestartPlayer(AController* NewPlayer) override;
    virtual FString InitNewPlayer(APlayerController* NewPlayer, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT("")) override;

private:
    bool ShouldUseOfflineEditorPlayProfile() const;
    void UpdateOnlinePlayerCount();
    void SaveAllPlayerProfiles();
    void HandleProfileLoaded(APlayerController* Player, bool bSuccess);

    FTimerHandle ProfileCheckpointTimer;
    TSet<AController*> ProfileReadyPlayers;

    UPROPERTY(Config, EditDefaultsOnly, Category="Grand City|Development")
    bool bAllowOfflineStandalonePIE = true;
};
