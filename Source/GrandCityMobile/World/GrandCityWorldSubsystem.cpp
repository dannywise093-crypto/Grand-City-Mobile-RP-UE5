#include "GrandCityWorldSubsystem.h"

#include "Engine/World.h"
#include "GrandCityMobileGameMode.h"
#include "GrandCityProceduralCity.h"
#include "GameFramework/GameModeBase.h"

void UGrandCityWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    bCityPrototypeSpawned = false;

    if (InWorld.WorldType != EWorldType::Game && InWorld.WorldType != EWorldType::PIE)
    {
        return;
    }

    // Template/variant maps bring their own game modes and level geometry.  The
    // Grand City prototype belongs only to worlds running the Grand City mode.
    // Spawn it on authority; the actor replicates and generates the same seeded
    // city for connected clients.
    if (InWorld.GetNetMode() == NM_Client
        || !InWorld.GetAuthGameMode<AGrandCityMobileGameMode>())
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    if (AGrandCityProceduralCity* City = InWorld.SpawnActor<AGrandCityProceduralCity>(
        AGrandCityProceduralCity::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams))
    {
        bCityPrototypeSpawned = true;
        UE_LOG(LogTemp, Display, TEXT("Spawned Grand City procedural city %s in %s."),
            *City->GetName(), *InWorld.GetMapName());
    }
}
