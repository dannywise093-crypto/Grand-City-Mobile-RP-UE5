#include "GrandCityMobileGameMode.h"
#include "GrandCityMobileCharacter.h"
#include "GrandCityMobilePlayerState.h"
#include "GrandCityMobileGameState.h"

AGrandCityMobileGameMode::AGrandCityMobileGameMode()
{
    DefaultPawnClass = AGrandCityMobileCharacter::StaticClass();
    PlayerStateClass = AGrandCityMobilePlayerState::StaticClass();
    GameStateClass = AGrandCityMobileGameState::StaticClass();
}
