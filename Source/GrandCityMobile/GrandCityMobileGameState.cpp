#include "GrandCityMobileGameState.h"
#include "Net/UnrealNetwork.h"

AGrandCityMobileGameState::AGrandCityMobileGameState()
{
    bReplicates = true;
}

void AGrandCityMobileGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeReplicatedProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeReplicatedProps);

    DOREPLIFETIME(AGrandCityMobileGameState, OnlinePlayerCount);
    DOREPLIFETIME(AGrandCityMobileGameState, ServerPopulationLimit);
    DOREPLIFETIME(AGrandCityMobileGameState, RegionId);
    DOREPLIFETIME(AGrandCityMobileGameState, ServerId);
    DOREPLIFETIME(AGrandCityMobileGameState, bServerDraining);
}
