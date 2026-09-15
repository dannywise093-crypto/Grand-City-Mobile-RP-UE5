// Grand City Mobile - lightweight mobile NPC interaction bridge.
#include "AI/GrandCityNPCInteractionComponent.h"
#include "GameFramework/Actor.h"

UGrandCityNPCInteractionComponent::UGrandCityNPCInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UGrandCityNPCInteractionComponent::CanPlayerInteract(AActor* PlayerActor) const
{
    if (!bInteractionEnabled || !PlayerActor || !GetOwner())
    {
        return false;
    }

    const float DistanceSquared = FVector::DistSquared(GetOwner()->GetActorLocation(), PlayerActor->GetActorLocation());
    const float SafeDistance = FMath::Max(50.0f, InteractionDistance);
    return DistanceSquared <= FMath::Square(SafeDistance);
}

void UGrandCityNPCInteractionComponent::SetInteractionEnabled(bool bEnabled)
{
    if (bInteractionEnabled == bEnabled)
    {
        return;
    }

    bInteractionEnabled = bEnabled;
    OnInteractionChanged.Broadcast(bInteractionEnabled);
}
