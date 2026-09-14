#include "GrandCityMobilePlayerController.h"

#include "GameFramework/Pawn.h"

AGrandCityMobilePlayerController::AGrandCityMobilePlayerController()
{
    bReplicates = true;
}

void AGrandCityMobilePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocalController())
    {
        ClientInitializeSession();
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

void AGrandCityMobilePlayerController::ClientInitializeSession_Implementation()
{
    bSessionInitialized = true;
}

void AGrandCityMobilePlayerController::ClientNotifySpawned_Implementation()
{
    bSpawnConfirmed = true;
}
