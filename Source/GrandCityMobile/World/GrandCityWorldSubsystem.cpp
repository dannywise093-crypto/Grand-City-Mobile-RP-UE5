#include "GrandCityWorldSubsystem.h"

#include "Engine/World.h"
#include "GrandCityProceduralCity.h"

void UGrandCityWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    bCityPrototypeSpawned = false;

    if (InWorld.WorldType != EWorldType::Game && InWorld.WorldType != EWorldType::PIE)
    {
        return;
    }

    if (InWorld.GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    if (InWorld.SpawnActor<AGrandCityProceduralCity>(AGrandCityProceduralCity::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams))
    {
        bCityPrototypeSpawned = true;
    }
}
