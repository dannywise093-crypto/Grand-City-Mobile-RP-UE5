#include "GrandCityMobileGameMode.h"
#include "GrandCityMobileCharacter.h"
#include "GrandCityMobileGameState.h"
#include "GrandCityMobilePlayerController.h"
#include "GrandCityMobilePlayerState.h"
#include "GrandCityAccountAuthSubsystem.h"
#include "GrandCityServerRegistrySubsystem.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
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

        if (UGameInstance* GameInstance = GetGameInstance())
        {
            if (UGrandCityServerRegistrySubsystem* Registry = GameInstance->GetSubsystem<UGrandCityServerRegistrySubsystem>())
            {
                Registry->StartServerRegistration();
            }
        }
    }
}

void AGrandCityMobileGameMode::BeginServerDrain()
{
    if (!HasAuthority())
    {
        return;
    }

    if (AGrandCityMobileGameState* CityGameState = GetGameState<AGrandCityMobileGameState>())
    {
        CityGameState->bServerDraining = true;
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UGrandCityServerRegistrySubsystem* Registry = GameInstance->GetSubsystem<UGrandCityServerRegistrySubsystem>())
        {
            Registry->BeginDraining();
        }
    }

    UE_LOG(LogTemp, Display, TEXT("Grand City server is now DRAINING; new player sessions will be rejected."));
}

FString AGrandCityMobileGameMode::InitNewPlayer(APlayerController* NewPlayer, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
    if (const AGrandCityMobileGameState* CityGameState = GetGameState<AGrandCityMobileGameState>())
    {
        if (CityGameState->bServerDraining)
        {
            return TEXT("SERVER_DRAINING");
        }
    }

    const FString Result = Super::InitNewPlayer(NewPlayer, UniqueId, Options, Portal);

    if (AGrandCityMobilePlayerController* CityController = Cast<AGrandCityMobilePlayerController>(NewPlayer))
    {
        FString AuthToken;
        FString TransferToken;
        FParse::Value(*Options, TEXT("AuthToken="), AuthToken);
        FParse::Value(*Options, TEXT("TransferToken="), TransferToken);
        CityController->SetAuthCredentials(AuthToken, TransferToken);
    }

    return Result;
}

void AGrandCityMobileGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (const AGrandCityMobileGameState* CityGameState = GetGameState<AGrandCityMobileGameState>())
    {
        if (CityGameState->bServerDraining)
        {
            RejectUnauthenticatedPlayer(Cast<AGrandCityMobilePlayerController>(NewPlayer), TEXT("This server is draining and is not accepting new players."));
            return;
        }
    }

    AGrandCityMobilePlayerController* CityController = Cast<AGrandCityMobilePlayerController>(NewPlayer);
    if (!CityController)
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UGrandCityAccountAuthSubsystem* Auth = GameInstance ? GameInstance->GetSubsystem<UGrandCityAccountAuthSubsystem>() : nullptr;
    if (!Auth || CityController->GetAuthToken().IsEmpty())
    {
        RejectUnauthenticatedPlayer(CityController, TEXT("Authentication is required. Please sign in again."));
        return;
    }

    TWeakObjectPtr<AGrandCityMobilePlayerController> WeakController(CityController);
    Auth->VerifyToken(CityController->GetAuthToken(),
        [this, WeakController](bool bVerified, const FGrandCityAccountIdentity& Identity)
        {
            AGrandCityMobilePlayerController* Player = WeakController.Get();
            if (!Player)
            {
                return;
            }

            if (!bVerified || Identity.AccountId.IsEmpty())
            {
                RejectUnauthenticatedPlayer(Player, TEXT("Your account session is invalid or expired."));
                return;
            }

            AGrandCityMobilePlayerState* PlayerState = Player->GetPlayerState<AGrandCityMobilePlayerState>();
            if (!PlayerState)
            {
                RejectUnauthenticatedPlayer(Player, TEXT("Unable to initialize account state."));
                return;
            }

            PlayerState->AccountId = Identity.AccountId;
            PlayerState->DisplayName = Identity.DisplayName;
            PlayerState->RegionId = Identity.RegionId;
            PlayerState->bAuthenticated = false;

            AGrandCityMobileGameState* CityGameState = GetGameState<AGrandCityMobileGameState>();
            const FString ServerId = CityGameState ? CityGameState->ServerId.ToString() : TEXT("GC-AFRICA-01");

            UGameInstance* GameInstance = GetGameInstance();
            UGrandCityAccountAuthSubsystem* Auth = GameInstance ? GameInstance->GetSubsystem<UGrandCityAccountAuthSubsystem>() : nullptr;
            if (!Auth)
            {
                RejectUnauthenticatedPlayer(Player, TEXT("Account service is unavailable."));
                return;
            }

            Auth->ClaimSession(Identity.AccountId, ServerId, Player->GetTransferToken(),
                [this, WeakController](bool bClaimed, const FString&)
                {
                    AGrandCityMobilePlayerController* ClaimedPlayer = WeakController.Get();
                    if (!ClaimedPlayer)
                    {
                        return;
                    }

                    if (!bClaimed)
                    {
                        RejectUnauthenticatedPlayer(ClaimedPlayer, TEXT("This account is already active on another server or the transfer is invalid."));
                        return;
                    }

                    ClaimedPlayer->LoadPersistentProfile(
                        [this, WeakController](bool bLoaded)
                        {
                            AGrandCityMobilePlayerController* LoadedPlayer = WeakController.Get();
                            if (LoadedPlayer)
                            {
                                HandleProfileLoaded(LoadedPlayer, bLoaded);
                            }
                        });
                });
        });
}

void AGrandCityMobileGameMode::Logout(AController* Exiting)
{
    ProfileReadyPlayers.Remove(Exiting);

    if (AGrandCityMobilePlayerController* CityController = Cast<AGrandCityMobilePlayerController>(Exiting))
    {
        const FString AccountId = CityController->GetPlayerState<AGrandCityMobilePlayerState>()
            ? CityController->GetPlayerState<AGrandCityMobilePlayerState>()->AccountId
            : FString();
        const FString ServerId = GetGameState<AGrandCityMobileGameState>()
            ? GetGameState<AGrandCityMobileGameState>()->ServerId.ToString()
            : TEXT("GC-AFRICA-01");

        CityController->SavePersistentProfile([this, AccountId, ServerId](bool)
        {
            UGameInstance* GameInstance = GetGameInstance();
            UGrandCityAccountAuthSubsystem* Auth = GameInstance ? GameInstance->GetSubsystem<UGrandCityAccountAuthSubsystem>() : nullptr;
            if (Auth && !AccountId.IsEmpty())
            {
                Auth->ReleaseSession(AccountId, ServerId, [](bool) {});
            }
        });
        CityController->ClearAuthCredentials();
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
        RejectUnauthenticatedPlayer(Cast<AGrandCityMobilePlayerController>(Player), TEXT("Unable to load your saved character data."));
        return;
    }

    if (AGrandCityMobilePlayerState* PlayerState = Player->GetPlayerState<AGrandCityMobilePlayerState>())
    {
        PlayerState->bAuthenticated = true;
    }

    ProfileReadyPlayers.Add(Player);
    RestartPlayer(Player);
}

void AGrandCityMobileGameMode::RejectUnauthenticatedPlayer(AGrandCityMobilePlayerController* Player, const FString& Reason)
{
    if (!Player || !HasAuthority())
    {
        return;
    }

    Player->ClientReturnToMainMenuWithTextReason(FText::FromString(Reason));
    Player->Destroy();
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
