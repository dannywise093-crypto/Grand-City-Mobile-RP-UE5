#include "GrandCityMobilePlayerState.h"
#include "Net/UnrealNetwork.h"

AGrandCityMobilePlayerState::AGrandCityMobilePlayerState()
{
    bReplicates = true;
}

void AGrandCityMobilePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeReplicatedProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeReplicatedProps);

    DOREPLIFETIME(AGrandCityMobilePlayerState, AccountId);
    DOREPLIFETIME(AGrandCityMobilePlayerState, DisplayName);
    DOREPLIFETIME(AGrandCityMobilePlayerState, RegionId);
    DOREPLIFETIME(AGrandCityMobilePlayerState, bAuthenticated);
    DOREPLIFETIME(AGrandCityMobilePlayerState, CharacterLevel);
    DOREPLIFETIME(AGrandCityMobilePlayerState, Cash);
    DOREPLIFETIME(AGrandCityMobilePlayerState, BankBalance);
    DOREPLIFETIME(AGrandCityMobilePlayerState, Reputation);
}
