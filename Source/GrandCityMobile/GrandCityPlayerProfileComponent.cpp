#include "GrandCityPlayerProfileComponent.h"

#include "GrandCityMobilePlayerState.h"
#include "GrandCityDurablePersistenceSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

namespace
{
    AGrandCityMobilePlayerState* GetOwningPlayerState(const UGrandCityPlayerProfileComponent* Component)
    {
        const APlayerController* PlayerController = Cast<APlayerController>(Component ? Component->GetOwner() : nullptr);
        return PlayerController ? PlayerController->GetPlayerState<AGrandCityMobilePlayerState>() : nullptr;
    }
}

UGrandCityPlayerProfileComponent::UGrandCityPlayerProfileComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UGrandCityPlayerProfileComponent::LoadProfile(FGrandCityProfileComponentLoadResult Callback)
{
    AGrandCityMobilePlayerState* PlayerState = GetOwningPlayerState(this);
    UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UGrandCityDurablePersistenceSubsystem* Persistence = GameInstance ? GameInstance->GetSubsystem<UGrandCityDurablePersistenceSubsystem>() : nullptr;

    if (!PlayerState || PlayerState->AccountId.IsEmpty() || !Persistence)
    {
        Callback(false);
        return;
    }

    Persistence->LoadProfile(PlayerState->AccountId,
        [this, Callback](bool bSuccess, bool bFound, const FGrandCityPlayerProfile& LoadedProfile)
        {
            if (!bSuccess)
            {
                Callback(false);
                return;
            }

            if (bFound)
            {
                Profile = LoadedProfile;
            }
            else
            {
                AGrandCityMobilePlayerState* PlayerState = GetOwningPlayerState(this);
                if (!PlayerState)
                {
                    Callback(false);
                    return;
                }

                Profile = FGrandCityPlayerProfile();
                Profile.AccountId = PlayerState->AccountId;
                Profile.CharacterId = FString::Printf(TEXT("CHAR-%s"), *PlayerState->AccountId);
                Profile.CharacterName = PlayerState->DisplayName;
                Profile.RegionId = PlayerState->RegionId;
                Profile.CharacterLevel = 1;
            }

            ApplyProfileToPlayerState();
            Callback(true);
        });
}

void UGrandCityPlayerProfileComponent::SaveProfile(FGrandCityProfileComponentSaveResult Callback)
{
    CaptureProfileFromPlayerState();
    if (Profile.AccountId.IsEmpty())
    {
        Callback(false);
        return;
    }

    Profile.LastSaveUnixSeconds = FDateTime::UtcNow().ToUnixTimestamp();

    UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UGrandCityDurablePersistenceSubsystem* Persistence = GameInstance ? GameInstance->GetSubsystem<UGrandCityDurablePersistenceSubsystem>() : nullptr;
    if (!Persistence)
    {
        Callback(false);
        return;
    }

    Persistence->SaveProfile(Profile, [Callback](bool bSuccess, const FGrandCityPlayerProfile&)
    {
        Callback(bSuccess);
    });
}

void UGrandCityPlayerProfileComponent::ApplyProfileToPlayerState()
{
    AGrandCityMobilePlayerState* PlayerState = GetOwningPlayerState(this);
    if (!PlayerState)
    {
        return;
    }

    PlayerState->CharacterLevel = FMath::Max(1, Profile.CharacterLevel);
    PlayerState->Cash = FMath::Max<int64>(0, Profile.Cash);
    PlayerState->BankBalance = FMath::Max<int64>(0, Profile.BankBalance);
    PlayerState->Reputation = Profile.Reputation;
    if (!Profile.CharacterName.IsEmpty())
    {
        PlayerState->DisplayName = Profile.CharacterName;
    }
    if (!Profile.RegionId.IsEmpty())
    {
        PlayerState->RegionId = Profile.RegionId;
    }
}

void UGrandCityPlayerProfileComponent::CaptureProfileFromPlayerState()
{
    AGrandCityMobilePlayerState* PlayerState = GetOwningPlayerState(this);
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
