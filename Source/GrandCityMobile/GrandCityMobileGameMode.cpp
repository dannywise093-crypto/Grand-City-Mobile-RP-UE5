#include "GrandCityMobileGameMode.h"
#include "GrandCityMobileCharacter.h"
#include "GrandCityMobileGameState.h"
#include "GrandCityMobilePlayerController.h"
#include "GrandCityMobilePlayerState.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

AGrandCityMobileGameMode::AGrandCityMobileGameMode()
{
    DefaultPawnClass = AGrandCityMobileCharacter::StaticClass();
    PlayerControllerClass = AGrandCityMobilePlayerController::StaticClass();
    PlayerStateClass = AGrandCityMobilePlayerState::StaticClass();
    GameStateClass = AGrandCityMobileGameState::StaticClass();
}

void AGrandCityMobileGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (!NewPlayer)
    {
        return;
    }

    UpdateOnlinePlayerCount();
}

void AGrandCityMobileGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);

    UpdateOnlinePlayerCount();
}

void AGrandCityMobileGameMode::RestartPlayer(AController* NewPlayer)
{
    if (!NewPlayer || !HasAuthority())
    {
        return;
    }

    Super::RestartPlayer(NewPlayer);
}

void AGrandCityMobileGameMode::UpdateOnlinePlayerCount()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    AGrandCityMobileGameState* CityGameState = GetGameState<AGrandCityMobileGameState>();
    if (!CityGameState)
    {
        return;
    }

    CityGameState->OnlinePlayerCount = GetNumPlayers();
}
