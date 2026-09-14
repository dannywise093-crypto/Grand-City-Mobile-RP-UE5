#include "GrandCityPlayerProfileComponent.h"

#include "GrandCityMobilePlayerState.h"
#include "GrandCityPlayerPersistenceSubsystem.h"
#include "Engine/GameInstance.h"

UGrandCityPlayerProfileComponent::UGrandCityPlayerProfileComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UGrandCityPlayerProfileComponent::LoadProfile()
{
    AGrandCityMobilePlayerState* PlayerState = Cast<AGrandCityMobilePlayerState>(GetOwner() ? GetOwner()->GetInstigatorController() ? GetOwner()->GetInstigatorController()->PlayerState : nullptr : nullptr);
    if (!PlayerState || PlayerState->AccountId.IsEmpty())
    {
        return false;
    }

    UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UGrandCityPlayerPersistenceSubsystem* Persistence = GameInstance ? GameInstance->GetSubsystem<UGrandCityPlayerPersistenceSubsystem>() : nullptr;
    if (!Persistence)
    {
        return false;
    }

    const bool bFound = Persistence->LoadProfile(PlayerState->AccountId, Profile);
    if (!bFound)
    {
        Profile.AccountId = PlayerState->AccountId;
        Profile.CharacterId = FString::Printf(TEXT("CHAR-%s"), *PlayerState->AccountId);
        Profile.CharacterName = PlayerState->DisplayName;
        Profile.RegionId = PlayerState->RegionId;
        Profile.CharacterLevel = PlayerState->CharacterLevel;
        Profile.Cash = PlayerState->Cash;
        Profile.BankBalance = PlayerState->BankBalance;
        Profile.Reputation = PlayerState->Reputation;
    }

    ApplyProfileToPlayerState();
    return true;
}

bool UGrandCityPlayerProfileComponent::SaveProfile()
{
    CaptureProfileFromPlayerState();
    if (Profile.AccountId.IsEmpty())
    {
        return false;
    }

    Profile.LastSaveUnixSeconds = FDateTime::UtcNow().ToUnixTimestamp();

    UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UGrandCityPlayerPersistenceSubsystem* Persistence = GameInstance ? GameInstance->GetSubsystem<UGrandCityPlayerPersistenceSubsystem>() : nullptr;
    return Persistence ? Persistence->SaveProfile(Profile) : false;
}

void UGrandCityPlayerProfileComponent::ApplyProfileToPlayerState()
{
    AGrandCityMobilePlayerState* PlayerState = Cast<AGrandCityMobilePlayerState>(GetOwner() ? GetOwner()->GetInstigatorController() ? GetOwner()->GetInstigatorController()->PlayerState : nullptr : nullptr);
    if (!PlayerState)
    {
        return;
    }

    PlayerState->CharacterLevel = FMath::Max(1, Profile.CharacterLevel);
    PlayerState->Cash = FMath::Max<int64>(0, Profile.Cash);
    PlayerState->BankBalance = FMath::Max<int64>(0, Profile.BankBalance);
    PlayerState->Reputation = Profile.Reputation;
}

void UGrandCityPlayerProfileComponent::CaptureProfileFromPlayerState()
{
    AGrandCityMobilePlayerState* PlayerState = Cast<AGrandCityMobilePlayerState>(GetOwner() ? GetOwner()->GetInstigatorController() ? GetOwner()->GetInstigatorController()->PlayerState : nullptr : nullptr);
    if (!PlayerState)
    {
        return;
    }

    Profile.AccountId = PlayerState->AccountId;
    Profile.CharacterName = PlayerState->DisplayName;
    Profile.RegionId = PlayerState->RegionId;
    Profile.CharacterLevel = PlayerState->CharacterLevel;
    Profile.Cash = PlayerState->Cash;
    Profile.BankBalance = PlayerState->BankBalance;
    Profile.Reputation = PlayerState->Reputation;
}
