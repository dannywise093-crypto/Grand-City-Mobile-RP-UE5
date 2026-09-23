#include "GrandCityWorldSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
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

    // Template/variant maps bring their own game modes and level geometry. The
    // city belongs only to worlds running the Grand City mode.
    if (!InWorld.GetAuthGameMode<AGrandCityMobileGameMode>())
    {
        return;
    }

    // The city is baked into the level. Never spawn or regenerate it here: a
    // runtime spawn was the reason the old prototype refreshed on every Play.
    for (TActorIterator<AGrandCityProceduralCity> It(&InWorld); It; ++It)
    {
        bCityPrototypeSpawned = true;
        UE_LOG(LogTemp, Display, TEXT("Using baked Grand City actor %s in %s; runtime spawning is disabled."),
            *It->GetName(), *InWorld.GetMapName());
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("No baked Grand City actor exists in %s. Place AGrandCityProceduralCity and press Regenerate City in the editor."),
        *InWorld.GetMapName());
}
