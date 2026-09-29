#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrandCityQuestTarget.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * A point of interest used by quest objectives: a destination, a delivery pickup/drop-off,
 * a collectible, a destructible object or something to interact with. Place it in the level
 * and add it to an objective's Targets list on a Quest Giver. What it does depends on the
 * objective that references it, so one target can serve different quest types.
 *
 * Reach/Deliver/Collect/Interact progress is per player: the target is only shown to players
 * whose active objective needs it. Destroy is shared: a destroyed target is gone for everyone
 * until it respawns.
 */
UCLASS()
class GRANDCITYMOBILE_API AGrandCityQuestTarget : public AActor
{
    GENERATED_BODY()

public:
    AGrandCityQuestTarget();

    FText GetDisplayName() const;
    float GetRadius() const { return Radius; }
    bool IsDestroyed() const { return bDestroyed; }

    /** Server only. Returns true when this damage destroyed the target. */
    bool ApplyQuestDamage(float Damage);

    /** Server only. Brings a destroyed target back (quest reset or respawn timer). */
    void Respawn();

    /** Local only. bActive: the local player's quest currently needs this target. */
    void SetLocalQuestState(bool bActive);

    /** Name shown on the marker and in the tracker, e.g. "Warehouse" or "Package". */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Target")
    FText DisplayName;

    /** Players within this distance reach / collect / can interact with the target. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Target", meta=(ClampMin="10.0", Units="cm"))
    float Radius = 200.0f;

    /** Keep the mesh visible even when no quest needs it (use for blocking/world objects). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Target")
    bool bAlwaysVisible = false;

    /** Destroy objectives: hits needed = MaxHealth / damage. Interact deals 1 hit's worth (InteractDamage). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Target|Destroy", meta=(ClampMin="1.0"))
    float MaxHealth = 100.0f;

    /** Destroy objectives: pressing interact next to the target damages it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Target|Destroy")
    bool bDamageByInteract = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Target|Destroy", meta=(EditCondition="bDamageByInteract", ClampMin="1.0"))
    float InteractDamage = 100.0f;

    /** Destroy objectives: driving into the target fast enough destroys it outright. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Target|Destroy")
    bool bDestroyByVehicleRam = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Target|Destroy",
        meta=(EditCondition="bDestroyByVehicleRam", ClampMin="0.0", Units="cm/s"))
    float MinRamSpeed = 600.0f;

    /** Seconds before a destroyed target comes back for other players. 0 = only on quest reset. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Target|Destroy", meta=(ClampMin="0.0", Units="s"))
    float RespawnDelaySeconds = 30.0f;

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    UFUNCTION()
    void OnRep_Destroyed();

    void RefreshVisuals();

    /** Editor-only view of Radius; never collides. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest Target")
    TObjectPtr<USphereComponent> RadiusSphere;

    /** Replace the mesh/material/collision freely. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest Target")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest Target")
    TObjectPtr<UTextRenderComponent> Label;

    UPROPERTY(ReplicatedUsing=OnRep_Destroyed, BlueprintReadOnly, Category="Quest Target")
    bool bDestroyed = false;

    float Health = 0.0f;
    bool bLocallyActive = false;
    ECollisionEnabled::Type MeshCollision = ECollisionEnabled::NoCollision;
    FTimerHandle RespawnTimer;
};
