#include "GrandCityMobileGameMode.h"
#include "GrandCityMobileCharacter.h"

AGrandCityMobileGameMode::AGrandCityMobileGameMode()
{
    DefaultPawnClass = AGrandCityMobileCharacter::StaticClass();
}
