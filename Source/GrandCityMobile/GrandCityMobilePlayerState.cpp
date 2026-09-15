#include "GrandCityMobilePlayerState.h"
#include "Net/UnrealNetwork.h"

AGrandCityMobilePlayerState::AGrandCityMobilePlayerState()
{
    bReplicates = true;
}

void AGrandCityMobilePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AGrandCityMobilePlayerState, AccountId);
    DOREPLIFETIME(AGrandCityMobilePlayerState, DisplayName);
    DOREPLIFETIME(AGrandCityMobilePlayerState, RegionId);
    DOREPLIFETIME(AGrandCityMobilePlayerState, bAuthenticated);
    DOREPLIFETIME(AGrandCityMobilePlayerState, CharacterLevel);
    DOREPLIFETIME(AGrandCityMobilePlayerState, Cash);
    DOREPLIFETIME(AGrandCityMobilePlayerState, BankBalance);
    DOREPLIFETIME(AGrandCityMobilePlayerState, Reputation);
}
