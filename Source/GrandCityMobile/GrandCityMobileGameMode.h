#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GrandCityMobileGameMode.generated.h"

class AController;
class APlayerController;

UCLASS()
class GRANDCITYMOBILE_API AGrandCityMobileGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AGrandCityMobileGameMode();

protected:
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void RestartPlayer(AController* NewPlayer) override;

private:
    void UpdateOnlinePlayerCount();
};
