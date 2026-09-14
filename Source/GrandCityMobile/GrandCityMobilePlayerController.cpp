#include "GrandCityMobilePlayerController.h"

#include "GrandCityPlayerProfileComponent.h"
#include "GrandCityMobileAuthWidget.h"
#include "GrandCityMobileAccountClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"

AGrandCityMobilePlayerController::AGrandCityMobilePlayerController()
{
    bReplicates = true;
    PlayerProfileComponent = CreateDefaultSubobject<UGrandCityPlayerProfileComponent>(TEXT("PlayerProfileComponent"));
}

void AGrandCityMobilePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocalController())
    {
        ClientInitializeSession();

        if (UGameInstance* GI = GetGameInstance())
        {
            if (UGrandCityMobileAccountClientSubsystem* AccountClient = GI->GetSubsystem<UGrandCityMobileAccountClientSubsystem>())
            {
                if (!AccountClient->IsAuthenticated())
                {
                    AuthWidget = CreateWidget<UGrandCityMobileAuthWidget>(this, UGrandCityMobileAuthWidget::StaticClass());
                    if (AuthWidget)
                    {
                        AuthWidget->AddToViewport(1000);
                    }
                }
            }
        }
    }
}

void AGrandCityMobilePlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (IsLocalController())
    {
        ClientNotifySpawned();
    }
}

void AGrandCityMobilePlayerController::OnUnPossess()
{
    bSpawnConfirmed = false;
    Super::OnUnPossess();
}

void AGrandCityMobilePlayerController::LoadPersistentProfile(FGrandCityProfileComponentLoadResult Callback)
{
    if (!HasAuthority() || !PlayerProfileComponent)
    {
        Callback(false);
        return;
    }

    PlayerProfileComponent->LoadProfile(MoveTemp(Callback));
}

void AGrandCityMobilePlayerController::SavePersistentProfile(FGrandCityProfileComponentSaveResult Callback)
{
    if (!HasAuthority() || !PlayerProfileComponent)
    {
        Callback(false);
        return;
    }

    PlayerProfileComponent->SaveProfile(MoveTemp(Callback));
}

void AGrandCityMobilePlayerController::SetAuthCredentials(const FString& InAuthToken, const FString& InTransferToken)
{
    AuthToken = InAuthToken;
    TransferToken = InTransferToken;
}

void AGrandCityMobilePlayerController::ClearAuthCredentials()
{
    AuthToken.Reset();
    TransferToken.Reset();
}

void AGrandCityMobilePlayerController::ClientInitializeSession_Implementation()
{
    bSessionInitialized = true;
}

void AGrandCityMobilePlayerController::ClientNotifySpawned_Implementation()
{
    bSpawnConfirmed = true;
}
