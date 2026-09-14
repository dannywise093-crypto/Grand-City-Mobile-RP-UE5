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
        GetWorldTimerManager().SetTimer(ProfileCheckpointTimer, this, &AGrandCityMobileGameMode::SaveAllPlayerProfiles, 60.0f, true, 60.0f);
        if (UGameInstance* GameInstance = GetGameInstance())
        {
            if (UGrandCityServerRegistrySubsystem* Registry = GameInstance->GetSubsystem<UGrandCityServerRegistrySubsystem>()) Registry->StartServerRegistration();
        }
    }
}

void AGrandCityMobileGameMode::BeginServerDrain()
{
    if (!HasAuthority()) return;
    if (AGrandCityMobileGameState* CityGameState = GetGameState<AGrandCityMobileGameState>()) CityGameState->bServerDraining = true;
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UGrandCityServerRegistrySubsystem* Registry = GameInstance->GetSubsystem<UGrandCityServerRegistrySubsystem>()) Registry->BeginDraining();
    }
    UE_LOG(LogTemp, Display, TEXT("Grand City server is now DRAINING; new player sessions will be rejected."));
}

FString AGrandCityMobileGameMode::InitNewPlayer(APlayerController* NewPlayer, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
    if (const AGrandCityMobileGameState* CityGameState = GetGameState<AGrandCityMobileGameState>())
    {
        if (CityGameState->bServerDraining) return TEXT("SERVER_DRAINING");
    }

    const FString Result = Super::InitNewPlayer(NewPlayer, UniqueId, Options, Portal);
    if (AGrandCityMobilePlayerController* CityController = Cast<AGrandCityMobilePlayerController>(NewPlayer))
    {
        FString AuthToken;
        FString TransferToken;
        FString ConnectionTicket;
        FParse::Value(*Options, TEXT("AuthToken="), AuthToken);
        FParse::Value(*Options, TEXT("TransferToken="), TransferToken);
        FParse::Value(*Options, TEXT("ConnectionTicket="), ConnectionTicket);
        CityController->SetAuthCredentials(AuthToken, TransferToken, ConnectionTicket);
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
    if (!CityController) return;

    UGameInstance* GameInstance = GetGameInstance();
    UGrandCityAccountAuthSubsystem* Auth = GameInstance ? GameInstance->GetSubsystem<UGrandCityAccountAuthSubsystem>() : nullptr;
    if (!Auth || CityController->GetConnectionTicket().IsEmpty())
    {
        RejectUnauthenticatedPlayer(CityController, TEXT("A secure connection ticket is required. Please sign in again."));
        return;
    }

    TWeakObjectPtr<AGrandCityMobilePlayerController> WeakController(CityController);
    const FString ServerId = GetGameState<AGrandCityMobileGameState>() ? GetGameState<AGrandCityMobileGameState>()->ServerId.ToString() : TEXT("GC-AFRICA-01");
    Auth->ConsumeConnectionTicket(CityController->GetConnectionTicket(), ServerId,
        [this, WeakController](bool bValid, const FString& AccountId)
        {
            AGrandCityMobilePlayerController* Player = WeakController.Get();
            if (!Player || !bValid || AccountId.IsEmpty())
            {
                if (Player) RejectUnauthenticatedPlayer(Player, TEXT("The connection ticket is invalid, expired, or belongs to another server."));
                return;
            }

            AGrandCityMobilePlayerState* PlayerState = Player->GetPlayerState<AGrandCityMobilePlayerState>();
            if (!PlayerState)
            {
                RejectUnauthenticatedPlayer(Player, TEXT("Unable to initialize account state."));
                return;
            }
            PlayerState->AccountId = AccountId;

            UGameInstance* GI = GetGameInstance();
            UGrandCityAccountAuthSubsystem* AccountAuth = GI ? GI->GetSubsystem<UGrandCityAccountAuthSubsystem>() : nullptr;
            if (!AccountAuth)
            {
                RejectUnauthenticatedPlayer(Player, TEXT("Account service is unavailable."));
                return;
            }

            const FString TargetServerId = GetGameState<AGrandCityMobileGameState>() ? GetGameState<AGrandCityMobileGameState>()->ServerId.ToString() : TEXT("GC-AFRICA-01");
            AccountAuth->ClaimSession(AccountId, TargetServerId, Player->GetTransferToken(),
                [this, WeakController](bool bClaimed, const FString&)
                {
                    AGrandCityMobilePlayerController* ClaimedPlayer = WeakController.Get();
                    if (!ClaimedPlayer) return;
                    if (!bClaimed)
                    {
                        RejectUnauthenticatedPlayer(ClaimedPlayer, TEXT("This account is already active on another server or the transfer is invalid."));
                        return;
                    }

                    ClaimedPlayer->LoadPersistentProfile([this, WeakController](bool bLoaded)
                    {
                        if (AGrandCityMobilePlayerController* LoadedPlayer = WeakController.Get()) HandleProfileLoaded(LoadedPlayer, bLoaded);
                    });
                });
        });
}

void AGrandCityMobileGameMode::Logout(AController* Exiting)
{
    ProfileReadyPlayers.Remove(Exiting);
    if (AGrandCityMobilePlayerController* CityController = Cast<AGrandCityMobilePlayerController>(Exiting))
    {
        const FString AccountId = CityController->GetPlayerState<AGrandCityMobilePlayerState>() ? CityController->GetPlayerState<AGrandCityMobilePlayerState>()->AccountId : FString();
        const FString ServerId = GetGameState<AGrandCityMobileGameState>() ? GetGameState<AGrandCityMobileGameState>()->ServerId.ToString() : TEXT("GC-AFRICA-01");
        CityController->SavePersistentProfile([this, AccountId, ServerId](bool)
        {
            if (UGameInstance* GameInstance = GetGameInstance())
            {
                if (UGrandCityAccountAuthSubsystem* Auth = GameInstance->GetSubsystem<UGrandCityAccountAuthSubsystem>())
                {
                    if (!AccountId.IsEmpty()) Auth->ReleaseSession(AccountId, ServerId, [](bool) {});
                }
            }
        });
        CityController->ClearAuthCredentials();
    }
    Super::Logout(Exiting);
    UpdateOnlinePlayerCount();
}

void AGrandCityMobileGameMode::RestartPlayer(AController* NewPlayer)
{
    if (!NewPlayer || !HasAuthority() || !ProfileReadyPlayers.Contains(NewPlayer)) return;
    Super::RestartPlayer(NewPlayer);
}

void AGrandCityMobileGameMode::HandleProfileLoaded(APlayerController* Player, bool bSuccess)
{
    if (!HasAuthority() || !Player) return;
    if (!bSuccess)
    {
        RejectUnauthenticatedPlayer(Cast<AGrandCityMobilePlayerController>(Player), TEXT("Unable to load your saved character data."));
        return;
    }
    if (AGrandCityMobilePlayerState* PlayerState = Player->GetPlayerState<AGrandCityMobilePlayerState>()) PlayerState->bAuthenticated = true;
    ProfileReadyPlayers.Add(Player);
    RestartPlayer(Player);
}

void AGrandCityMobileGameMode::RejectUnauthenticatedPlayer(AGrandCityMobilePlayerController* Player, const FString& Reason)
{
    if (!Player || !HasAuthority()) return;
    Player->ClientReturnToMainMenuWithTextReason(FText::FromString(Reason));
    Player->Destroy();
}

void AGrandCityMobileGameMode::UpdateOnlinePlayerCount()
{
    if (!HasAuthority() || !GetWorld()) return;
    if (AGrandCityMobileGameState* CityGameState = GetGameState<AGrandCityMobileGameState>()) CityGameState->OnlinePlayerCount = GetNumPlayers();
}

void AGrandCityMobileGameMode::SaveAllPlayerProfiles()
{
    if (!HasAuthority() || !GetWorld()) return;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (AGrandCityMobilePlayerController* CityController = Cast<AGrandCityMobilePlayerController>(It->Get())) CityController->SavePersistentProfile([](bool) {});
    }
}
