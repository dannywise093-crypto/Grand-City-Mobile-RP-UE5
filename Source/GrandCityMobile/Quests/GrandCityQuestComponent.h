#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Quests/GrandCityQuestTypes.h"
#include "GrandCityQuestComponent.generated.h"

class AGrandCityQuestCargo;
class AGrandCityQuestGiver;
class AGrandCityQuestTarget;
class AGrandCityVehicle;
class APlayerController;
class UGrandCityQuestOfferWidget;
class UGrandCityQuestTrackerWidget;

UENUM(BlueprintType)
enum class EGrandCityQuestEvent : uint8
{
    Started,
    ObjectiveCompleted,
    Completed,
    Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGrandCityQuestDelegate, AGrandCityQuestGiver*, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGrandCityQuestFailedDelegate, AGrandCityQuestGiver*, Quest, const FText&, Reason);

/**
 * Per-player quest state, owned by the player controller. The server runs the quest
 * (progress, timers, completion); the owning client gets the state replicated and drives
 * the offer window, the tracker HUD and which quest markers are visible.
 */
UCLASS(ClassGroup=(GrandCity), meta=(BlueprintSpawnableComponent))
class GRANDCITYMOBILE_API UGrandCityQuestComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGrandCityQuestComponent();

    /** Quest component of the player behind a controller, pawn (on foot or driving) or player state. */
    static UGrandCityQuestComponent* FindForActor(const AActor* Actor);

    /** Re-applies marker visibility for every local player (called when quest actors stream in). */
    static void RefreshLocalQuestVisuals(const UObject* WorldContextObject);

    // --- Local (owning client) ---------------------------------------------------------------

    /** Polled by the controller. Returns true while the interact button should offer a quest action. */
    bool UpdateLocalInteraction();
    FText GetLocalInteractionLabel() const;
    /** Handles an interact press. Returns false when no quest action was available. */
    bool TryLocalInteract();
    /** Answers the open offer window (Y / U keys). Returns false when no offer is open. */
    bool AcceptLocalOffer();
    bool DeclineLocalOffer();
    void RefreshLocalVisuals();

    // --- Server ------------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest")
    void StartQuest(AGrandCityQuestGiver* QuestGiver);

    /** Fails the active quest. Progress is reset; the quest has to be done again from the start. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest")
    void FailQuest(FText Reason);

    /** Progresses Custom objectives whose CustomEventTag matches. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest")
    void ReportQuestEvent(FName EventTag, int32 Amount = 1);

    /** ReportQuestEvent for the player behind PlayerActor (controller, pawn or vehicle). */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest", meta=(DefaultToSelf="PlayerActor"))
    static void ReportQuestEventForPlayer(AActor* PlayerActor, FName EventTag, int32 Amount = 1);

    /** Server only. Called by quest targets when this player damages them. */
    void HandleTargetDamaged(AGrandCityQuestTarget* Target, float Damage);

    // --- Queries -----------------------------------------------------------------------------

    UFUNCTION(BlueprintPure, Category="Quest")
    AGrandCityQuestGiver* GetActiveQuest() const { return ActiveQuest; }

    UFUNCTION(BlueprintPure, Category="Quest")
    const TArray<FGrandCityQuestObjectiveProgress>& GetObjectiveProgress() const { return ObjectiveProgress; }

    UFUNCTION(BlueprintPure, Category="Quest")
    bool HasCompletedQuest(FName QuestId) const { return CompletedQuestIds.Contains(QuestId); }

    const TArray<FName>& GetCompletedQuestIds() const { return CompletedQuestIds; }

    /** Seconds left on the whole-quest timer, or -1 when it has none. */
    UFUNCTION(BlueprintPure, Category="Quest")
    float GetRemainingQuestTime() const;

    /** Seconds left on an objective's timer, or -1 when it has none / is not running. */
    UFUNCTION(BlueprintPure, Category="Quest")
    float GetRemainingObjectiveTime(int32 ObjectiveIndex) const;

    /** The target an objective waits for next (ordered objectives), or null. */
    AGrandCityQuestTarget* GetNextOrderedTarget(int32 ObjectiveIndex) const;

    /**
     * Deliver objectives: the targets the player can drive to right now. bOutPickup tells
     * whether they are pickups (nothing loaded) or drop-offs for the loaded cargo.
     */
    void GetOpenDeliveryTargets(int32 ObjectiveIndex, TArray<AGrandCityQuestTarget*>& OutTargets, bool& bOutPickup) const;

    UPROPERTY(BlueprintAssignable, Category="Quest")
    FGrandCityQuestDelegate OnQuestStarted;

    UPROPERTY(BlueprintAssignable, Category="Quest")
    FGrandCityQuestDelegate OnQuestCompleted;

    UPROPERTY(BlueprintAssignable, Category="Quest")
    FGrandCityQuestFailedDelegate OnQuestFailed;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(Server, Reliable)
    void ServerAcceptQuest(AGrandCityQuestGiver* QuestGiver);

    UFUNCTION(Server, Reliable)
    void ServerInteractWithTarget(AGrandCityQuestTarget* Target);

    UFUNCTION(Client, Reliable)
    void ClientQuestEvent(AGrandCityQuestGiver* QuestGiver, EGrandCityQuestEvent Event, const FText& Detail);

    /** Opens the offer window for a follow-up quest regardless of where the player stands. */
    UFUNCTION(Client, Reliable)
    void ClientOfferQuest(AGrandCityQuestGiver* QuestGiver);

    UFUNCTION()
    void OnRep_QuestState();

private:
    APlayerController* GetOwningPlayerController() const;
    bool IsLocallyControlled() const;
    double GetServerTime() const;

    /** Which targets the player can progress right now. */
    bool IsTargetOpen(int32 ObjectiveIndex, int32 TargetIndex) const;
    static bool IsOrderedObjective(const FGrandCityQuestObjective& Objective);
    static int32 ComputeRequiredCount(const FGrandCityQuestObjective& Objective);
    /** Deliver: every pickup > drop-off leg of the objective's route, in list order. */
    static void GetDeliveryLegs(const FGrandCityQuestObjective& Objective, TArray<FGrandCityDeliveryLeg>& OutLegs);
    bool IsPawnInTargetRange(const APawn* Pawn, const AGrandCityQuestTarget* Target, float Tolerance = 0.0f) const;

    void TickQuest();
    void TickDelivery(int32 ObjectiveIndex, AGrandCityVehicle* Vehicle);
    void CompleteDelivery(int32 ObjectiveIndex, const AGrandCityQuestTarget* DropOff);
    void ActivateObjective(int32 ObjectiveIndex);
    void CompleteTarget(int32 ObjectiveIndex, int32 TargetIndex);
    void AddObjectiveProgress(int32 ObjectiveIndex, int32 Amount);
    void CompleteObjective(int32 ObjectiveIndex);
    void CompleteQuest();
    void CreditDestroyedTarget(AGrandCityQuestTarget* Target);
    void ClearActiveQuest();
    /** Brings back destroyed targets so a restarted quest begins from a clean state. */
    static void RespawnQuestTargets(const AGrandCityQuestGiver* QuestGiver);
    void BroadcastQuestEvent(AGrandCityQuestGiver* QuestGiver, EGrandCityQuestEvent Event, const FText& Detail);

    /** Local: puts each active Deliver objective's cargo at its pickup or on its vehicle. */
    void RefreshLocalCargo();

    void ShowOffer(AGrandCityQuestGiver* QuestGiver, bool bRemoteOffer);
    void CloseOffer();
    void HandleOfferAccepted();
    void HandleOfferDeclined();

    UPROPERTY(ReplicatedUsing=OnRep_QuestState)
    TObjectPtr<AGrandCityQuestGiver> ActiveQuest;

    UPROPERTY(ReplicatedUsing=OnRep_QuestState)
    TArray<FGrandCityQuestObjectiveProgress> ObjectiveProgress;

    /** Server world time the whole quest times out at; negative when it has no timer. */
    UPROPERTY(ReplicatedUsing=OnRep_QuestState)
    double QuestEndTime = -1.0;

    UPROPERTY(ReplicatedUsing=OnRep_QuestState)
    TArray<FName> CompletedQuestIds;

    /** Server: follow-up quest offered remotely, accepted without standing in its area. */
    TWeakObjectPtr<AGrandCityQuestGiver> PendingRemoteOffer;
    FTimerHandle QuestTickTimer;

    // Local UI state.
    UPROPERTY(Transient)
    TObjectPtr<UGrandCityQuestOfferWidget> OfferWidget;

    UPROPERTY(Transient)
    TObjectPtr<UGrandCityQuestTrackerWidget> TrackerWidget;

    /**
     * Cargo boxes of the active quest, keyed by objective index and the pickup they belong to.
     * The level keeps spawned actors alive, so weak pointers are enough.
     */
    TMap<TPair<int32, const AGrandCityQuestTarget*>, TWeakObjectPtr<AGrandCityQuestCargo>> CargoActors;

    TWeakObjectPtr<AGrandCityQuestGiver> OfferedQuest;
    TWeakObjectPtr<AGrandCityQuestGiver> NearbyQuestGiver;
    TWeakObjectPtr<AGrandCityQuestTarget> NearbyInteractTarget;
    bool bOfferIsRemote = false;
    bool bOfferChangedInputMode = false;
};
