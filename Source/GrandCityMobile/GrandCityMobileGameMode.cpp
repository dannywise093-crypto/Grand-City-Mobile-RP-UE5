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

void AGrandCityMobileGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority() && GetWorld())
    {
        GetWorldTimerManager().SetTimer(
            ProfileCheckpointTimer,
            this,
            &AGrandCityMobileGameMode::SaveAllPlayerProfiles,
            60.0f,
            true,
            60.0f);
    }
}

FString AGrandCityMobileGameMode::InitNewPlayer(APlayerController* NewPlayer, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
    const FString Result = Super::InitNewPlayer(NewPlayer, UniqueId, Options, Portal);

    if (NewPlayer)
    {
        AGrandCityMobilePlayerState* PlayerState = NewPlayer->GetPlayerState<AGrandCityMobilePlayerState>();
        if (PlayerState)
        {
            FString AccountId;
            FParse::Value(*Options, TEXT("AccountId="), AccountId);
            if (AccountId.IsEmpty() && UniqueId.IsValid())
            {
                AccountId = UniqueId.ToString();
            }
            if (AccountId.IsEmpty())
            {
                AccountId = FString::Printf(TEXT("LOCAL-%d"), PlayerState->GetPlayerId());
            }

            PlayerState->AccountId = AccountId;
            PlayerState->DisplayName = NewPlayer->GetName();
            PlayerState->RegionId = TEXT("AFRICA_WEST");
            PlayerState->bAuthenticated = false;
        }
    }

    return Result;
}

void AGrandCityMobileGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (!NewPlayer)
    {
        return;
    }

    AGrandCityMobilePlayerState* PlayerState = NewPlayer->GetPlayerState<AGrandCityMobilePlayerState>();
    if (!PlayerState || PlayerState->AccountId.IsEmpty())
    {
        NewPlayer->Destroy();
        return;
    }

    if (AGrandCityMobilePlayerController* CityController = Cast<AGrandCityMobilePlayerController>(NewPlayer))
    {
        TWeakObjectPtr<APlayerController> WeakPlayer(NewPlayer);
        CityController->LoadPersistentProfile(
            [this, WeakPlayer](bool bSuccess)
            {
                APlayerController* Player = WeakPlayer.Get();
                if (Player)
                {
                    HandleProfileLoaded(Player, bSuccess);
                }
            });
    }

    UpdateOnlinePlayerCount();
}

void AGrandCityMobileGameMode::Logout(AController* Exiting)
{
    ProfileReadyPlayers.Remove(Exiting);

    if (AGrandCityMobilePlayerController* CityController = Cast<AGrandCityMobilePlayerController>(Exiting))
    {
        CityController->SavePersistentProfile([](bool) {});
    }

    Super::Logout(Exiting);
    UpdateOnlinePlayerCount();
}

void AGrandCityMobileGameMode::RestartPlayer(AController* NewPlayer)
{
    if (!NewPlayer || !HasAuthority())
    {
        return;
    }

    if (!ProfileReadyPlayers.Contains(NewPlayer))
    {
        return;
    }

    Super::RestartPlayer(NewPlayer);
}

void AGrandCityMobileGameMode::HandleProfileLoaded(APlayerController* Player, bool bSuccess)
{
    if (!HasAuthority() || !Player)
    {
        return;
    }

    if (!bSuccess)
    {
        Player->ClientReturnToMainMenuWithTextReason(FText::FromString(TEXT("Unable to load your saved character data. Please try again.")));
        Player->Destroy();
        return;
    }

    if (AGrandCityMobilePlayerState* PlayerState = Player->GetPlayerState<AGrandCityMobilePlayerState>())
    {
        PlayerState->bAuthenticated = true;
    }

    ProfileReadyPlayers.Add(Player);
    RestartPlayer(Player);
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

void AGrandCityMobileGameMode::SaveAllPlayerProfiles()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (AGrandCityMobilePlayerController* CityController = Cast<AGrandCityMobilePlayerController>(It->Get()))
        {
            CityController->SavePersistentProfile([](bool) {});
        }
    }
}
