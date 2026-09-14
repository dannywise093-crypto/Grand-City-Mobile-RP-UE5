#include "GrandCityMobilePlayerController.h"

#include "GrandCityPlayerProfileComponent.h"
#include "GrandCityMobileAuthWidget.h"
#include "GrandCityMobileAccountClientSubsystem.h"
#include "GrandCityGlobalRoutingSubsystem.h"
#include "Engine/GameInstance.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Misc/ConfigCacheIni.h"
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

void AGrandCityMobilePlayerController::TravelToAuthenticatedRegion(const FString& RegionId)
{
    if (!IsLocalController() || AuthToken.IsEmpty())
    {
        return;
    }

    FString Address;
    const FString Section = TEXT("/Script/GrandCityMobile.GrandCityRegionalServers");
    GConfig->GetString(*Section, *RegionId, Address, GGameIni);

    if (Address.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("No regional server configured for region %s"), *RegionId);
        return;
    }

    FString TravelURL = Address + TEXT("?AuthToken=") + FGenericPlatformHttp::UrlEncode(AuthToken);
    if (!TransferToken.IsEmpty())
    {
        TravelURL += TEXT("&TransferToken=") + FGenericPlatformHttp::UrlEncode(TransferToken);
    }

    ClientTravel(TravelURL, TRAVEL_Absolute);
}

void AGrandCityMobilePlayerController::TravelToBestWorldwideServer()
{
    if (!IsLocalController() || AuthToken.IsEmpty())
    {
        return;
    }

    UGameInstance* GI = GetGameInstance();
    UGrandCityGlobalRoutingSubsystem* Router = GI ? GI->GetSubsystem<UGrandCityGlobalRoutingSubsystem>() : nullptr;
    UGrandCityMobileAccountClientSubsystem* AccountClient = GI ? GI->GetSubsystem<UGrandCityMobileAccountClientSubsystem>() : nullptr;
    if (!Router || !AccountClient)
    {
        UE_LOG(LogTemp, Error, TEXT("Global routing or account service unavailable; refusing fallback travel."));
        return;
    }

    Router->FindBestServer(TMap<FString, int32>(), [this, AccountClient](const FGrandCityRouteResult& Route)
    {
        if (!Route.bSuccess || Route.Endpoint.IsEmpty() || Route.ServerId.IsEmpty())
        {
            UE_LOG(LogTemp, Error, TEXT("Worldwide routing failed: %s"), *Route.Error);
            return;
        }

        AccountClient->RequestConnectionTicket(Route.ServerId, [this, Route](bool bTicketSuccess, const FString& Ticket, const FString& ErrorCode)
        {
            if (!bTicketSuccess || Ticket.IsEmpty())
            {
                UE_LOG(LogTemp, Error, TEXT("Connection ticket request failed: %s"), *ErrorCode);
                return;
            }

            FString TravelURL = Route.Endpoint + TEXT("?ConnectionTicket=") + FGenericPlatformHttp::UrlEncode(Ticket);
            if (!TransferToken.IsEmpty())
            {
                TravelURL += TEXT("&TransferToken=") + FGenericPlatformHttp::UrlEncode(TransferToken);
            }

            ClientTravel(TravelURL, TRAVEL_Absolute);
        });
    });
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

void AGrandCityMobilePlayerController::SetAuthCredentials(const FString& InAuthToken, const FString& InTransferToken, const FString& InConnectionTicket)
{
    AuthToken = InAuthToken;
    TransferToken = InTransferToken;
    ConnectionTicket = InConnectionTicket;
}

void AGrandCityMobilePlayerController::ClearAuthCredentials()
{
    AuthToken.Reset();
    TransferToken.Reset();
    ConnectionTicket.Reset();
}

void AGrandCityMobilePlayerController::ClientInitializeSession_Implementation()
{
    bSessionInitialized = true;
}

void AGrandCityMobilePlayerController::ClientNotifySpawned_Implementation()
{
    bSpawnConfirmed = true;
}
