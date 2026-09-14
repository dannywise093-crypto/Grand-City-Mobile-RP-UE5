#include "GrandCityGameMode.h"
#include "GrandCityMobileCharacter.h"
#include "GrandCityPlayerController.h"

AGrandCityGameMode::AGrandCityGameMode()
{
    DefaultPawnClass = AGrandCityMobileCharacter::StaticClass();
    PlayerControllerClass = AGrandCityPlayerController::StaticClass();
}
