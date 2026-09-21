#include "GrandCityMobileGameMode.h"
#include "GrandCityMobileCharacter.h"
#include "GrandCityMobileGameState.h"
#include "GrandCityMobilePlayerController.h"
#include "GrandCityMobilePlayerState.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Online/CoreOnline.h"
#include "TimerManager.h"

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

    if (HasAuthority() && GetWorld() && !ShouldUseOfflineEditorPlayProfile())
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

    if (ShouldUseOfflineEditorPlayProfile())
    {
        UE_LOG(LogTemp, Display, TEXT("Using an offline standalone development profile for player %s."), *PlayerState->AccountId);
        ProfileReadyPlayers.Add(NewPlayer);
        RestartPlayer(NewPlayer);
        UpdateOnlinePlayerCount();
        return;
    }

    if (AGrandCityMobilePlayerController* CityController = Cast<AGrandCityMobilePlayerController>(NewPlayer))
    {
        TWeakObjectPtr<AGrandCityMobileGameMode> WeakGameMode(this);
        TWeakObjectPtr<APlayerController> WeakPlayer(NewPlayer);
        CityController->LoadPersistentProfile(
            [WeakGameMode, WeakPlayer](bool bSuccess)
            {
                AGrandCityMobileGameMode* GameMode = WeakGameMode.Get();
                APlayerController* Player = WeakPlayer.Get();
                if (GameMode && Player && Player->GetWorld() == GameMode->GetWorld())
                {
                    GameMode->HandleProfileLoaded(Player, bSuccess);
                }
            });
    }

    UpdateOnlinePlayerCount();
}

void AGrandCityMobileGameMode::Logout(AController* Exiting)
{
    const bool bWasProfileReady = ProfileReadyPlayers.Remove(Exiting) > 0;

    if (bWasProfileReady && !ShouldUseOfflineEditorPlayProfile())
    {
        if (AGrandCityMobilePlayerController* CityController = Cast<AGrandCityMobilePlayerController>(Exiting))
        {
            CityController->SavePersistentProfile([](bool) {});
        }
    }

    Super::Logout(Exiting);
    UpdateOnlinePlayerCount();
}

bool AGrandCityMobileGameMode::ShouldUseOfflineEditorPlayProfile() const
{
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("GrandCityCrouchPlaytest")))
    {
        return true;
    }
#endif
#if WITH_EDITOR
    const UWorld* World = GetWorld();
    return bAllowOfflineStandalonePIE
        && World
        && (World->IsPlayInEditor() || World->IsPlayInPreview());
#elif PLATFORM_ANDROID && UE_BUILD_DEVELOPMENT
    const UWorld* World = GetWorld();
    return bAllowOfflineStandalonePIE
        && World
        && World->GetNetMode() == NM_Standalone;
#else
    return false;
#endif
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

    if (const APawn* SpawnedPawn = NewPlayer->GetPawn())
    {
        UE_LOG(LogTemp, Display, TEXT("Grand City player spawned with pawn %s (%s)."),
            *SpawnedPawn->GetName(), *SpawnedPawn->GetClass()->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Grand City failed to spawn a pawn for controller %s."),
            *NewPlayer->GetName());
    }
}

void AGrandCityMobileGameMode::HandleProfileLoaded(APlayerController* Player, bool bSuccess)
{
    if (!HasAuthority() || !Player)
    {
        return;
    }

    if (!bSuccess)
    {
        ProfileReadyPlayers.Remove(Player);

        if (AGrandCityMobilePlayerState* PlayerState = Player->GetPlayerState<AGrandCityMobilePlayerState>())
        {
            PlayerState->bAuthenticated = false;
        }

        UE_LOG(LogTemp, Error,
            TEXT("Unable to load the persistent profile for %s. The player will remain connected without a pawn."),
            *Player->GetName());
        Player->ClientMessage(TEXT("Unable to load your saved character data. Please check the persistence service and try again."));
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
    if (!HasAuthority() || !GetWorld() || ShouldUseOfflineEditorPlayProfile())
    {
        return;
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PlayerController = It->Get();
        if (ProfileReadyPlayers.Contains(PlayerController))
        {
            if (AGrandCityMobilePlayerController* CityController = Cast<AGrandCityMobilePlayerController>(PlayerController))
            {
                CityController->SavePersistentProfile([](bool) {});
            }
        }
    }
}
