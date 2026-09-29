#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrandCityQuestCargo.generated.h"

class AGrandCityVehicle;
class UStaticMeshComponent;

/**
 * The box a Deliver objective carries. It waits at the pickup target, hops onto the roof of
 * the vehicle that picks it up and is removed at the last drop-off.
 *
 * Local visual only: the owning player's quest component spawns and moves it from the
 * replicated quest state, so every player sees just their own cargo (like quest targets).
 */
UCLASS(NotPlaceable)
class GRANDCITYMOBILE_API AGrandCityQuestCargo : public AActor
{
    GENERATED_BODY()

public:
    AGrandCityQuestCargo();

    /** Sets the cargo down on the ground at a pickup point. */
    void PlaceAt(const FVector& GroundLocation, const FRotator& Rotation);

    /**
     * Attaches the cargo to the vehicle's roof so it rides along.
     * bAnimate: hop over from where it is now instead of appearing on the roof.
     */
    void LoadOnto(AGrandCityVehicle* Vehicle, bool bAnimate);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo")
    TObjectPtr<UStaticMeshComponent> Mesh;

private:
    void ApplyLoadMotion();

    // Relative to the vehicle while loading.
    FVector LoadStartLocation = FVector::ZeroVector;
    FQuat LoadStartRotation = FQuat::Identity;
    FVector LoadTargetLocation = FVector::ZeroVector;
    float LoadElapsed = 0.0f;
};
