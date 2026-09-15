#include "GrandCityMobileGameState.h"
#include "Net/UnrealNetwork.h"

AGrandCityMobileGameState::AGrandCityMobileGameState()
{
    bReplicates = true;
}

void AGrandCityMobileGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AGrandCityMobileGameState, OnlinePlayerCount);
    DOREPLIFETIME(AGrandCityMobileGameState, ServerPopulationLimit);
    DOREPLIFETIME(AGrandCityMobileGameState, RegionId);
    DOREPLIFETIME(AGrandCityMobileGameState, ServerId);
}
