#pragma once

#include "CoreMinimal.h"
#include "GrandCityQuestTypes.generated.h"

class AGrandCityQuestTarget;
class AGrandCityVehicle;

/** What the player has to do to progress an objective. */
UENUM(BlueprintType)
enum class EGrandCityQuestObjectiveType : uint8
{
    /** Walk or drive into every target's radius (optionally in list order). */
    ReachLocation,
    /**
     * Pick up cargo and drive it to drop-offs; the route is set by DeliveryRoute. Arriving at a
     * pickup by vehicle loads a cargo box onto its roof; drop-offs only count with that vehicle.
     */
    Deliver,
    /** Touch pickup targets to collect them. */
    Collect,
    /** Destroy targets (damage, ramming with a vehicle, or interact when allowed on the target). */
    Destroy,
    /** Press interact next to the targets (talk to someone, use a terminal...). */
    Interact,
    /** Progressed from Blueprint/C++ through UGrandCityQuestComponent::ReportQuestEvent. */
    Custom
};

UENUM(BlueprintType)
enum class EGrandCityQuestObjectiveStatus : uint8
{
    /** Waiting for earlier objectives (sequential quests only). */
    Locked,
    Active,
    Completed
};

/** How a Deliver objective strings its pickups and drop-offs together. */
UENUM(BlueprintType)
enum class EGrandCityDeliveryRoute : uint8
{
    /** Targets[0] is the pickup, loaded once; every other target is a stop. The cargo stays on until the last stop. */
    MultiDrop UMETA(DisplayName="Multi-Drop (A > B > C > D)"),
    /** Targets[0] is the depot; every other target gets its own load, so the player returns to the depot between stops. */
    ReturnToDepot UMETA(DisplayName="Return To Depot (A > B > A > C > A > D)"),
    /** Delivery Legs lists every pickup with its drop-off. A target may appear in several legs (A > A', A > C, ...). */
    PickupDropPairs UMETA(DisplayName="Pickup / Drop-off Pairs")
};

/** One delivery: load at Pickup, unload at DropOff. */
USTRUCT(BlueprintType)
struct GRANDCITYMOBILE_API FGrandCityDeliveryLeg
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Delivery")
    TObjectPtr<AGrandCityQuestTarget> Pickup;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Delivery")
    TObjectPtr<AGrandCityQuestTarget> DropOff;
};

/** One step of a quest. Edited per quest giver in the level Details panel. */
USTRUCT(BlueprintType)
struct GRANDCITYMOBILE_API FGrandCityQuestObjective
{
    GENERATED_BODY()

    /** Shown in the offer window and the quest tracker. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective", meta=(MultiLine="true"))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective")
    EGrandCityQuestObjectiveType Type = EGrandCityQuestObjectiveType::ReachLocation;

    /** Deliver only: how pickups and drop-offs are chained. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective",
        meta=(EditCondition="Type == EGrandCityQuestObjectiveType::Deliver", EditConditionHides))
    EGrandCityDeliveryRoute DeliveryRoute = EGrandCityDeliveryRoute::MultiDrop;

    /**
     * Quest Target actors placed in the level. Move them in the viewport to change where the
     * objective happens. Not used by Custom objectives or Pickup / Drop-off Pairs deliveries.
     * Multi-Drop / Return To Depot: Targets[0] is the pickup, the rest are drop-offs.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective",
        meta=(EditCondition="Type != EGrandCityQuestObjectiveType::Custom && (Type != EGrandCityQuestObjectiveType::Deliver || DeliveryRoute != EGrandCityDeliveryRoute::PickupDropPairs)", EditConditionHides))
    TArray<TObjectPtr<AGrandCityQuestTarget>> Targets;

    /** Pickup / Drop-off Pairs only: one entry per delivery. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective",
        meta=(EditCondition="Type == EGrandCityQuestObjectiveType::Deliver && DeliveryRoute == EGrandCityDeliveryRoute::PickupDropPairs", EditConditionHides))
    TArray<FGrandCityDeliveryLeg> DeliveryLegs;

    /**
     * ReachLocation: the targets must be visited from top to bottom of the list.
     * Deliver: deliveries are made in list order. Off: the player picks any open pickup
     * (and, once loaded, any drop-off that cargo can go to).
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective",
        meta=(EditCondition="Type == EGrandCityQuestObjectiveType::ReachLocation || Type == EGrandCityQuestObjectiveType::Deliver", EditConditionHides))
    bool bVisitTargetsInOrder = true;

    /**
     * How many targets/events are needed. 0 = all targets in the list.
     * Deliver: how many deliveries finish the objective (0 = every drop-off, 1 = a single delivery).
     * Custom objectives always need at least 1 event.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective", meta=(ClampMin="0"))
    int32 RequiredCount = 0;

    /** Custom only: the event name passed to ReportQuestEvent. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective",
        meta=(EditCondition="Type == EGrandCityQuestObjectiveType::Custom", EditConditionHides))
    FName CustomEventTag;

    /** ReachLocation/Collect: the target only counts when the player arrives driving a vehicle. Deliver always needs one. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective",
        meta=(EditCondition="Type == EGrandCityQuestObjectiveType::ReachLocation || Type == EGrandCityQuestObjectiveType::Collect", EditConditionHides))
    bool bRequireVehicle = false;

    /** Gives this objective its own countdown. Needs "Use Objective Timers" on the quest. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective|Timer")
    bool bUseTimer = false;

    /** Seconds from the moment this objective becomes active. Running out fails the quest. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective|Timer",
        meta=(EditCondition="bUseTimer", ClampMin="1.0", Units="s"))
    float TimeLimitSeconds = 60.0f;
};

/** Full quest description. Lives on AGrandCityQuestGiver so it is edited in the level. */
USTRUCT(BlueprintType)
struct GRANDCITYMOBILE_API FGrandCityQuestDefinition
{
    GENERATED_BODY()

    /** Unique id used to remember completed quests. Empty = the quest giver's actor name. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    FName QuestId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    FText Title;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta=(MultiLine="true"))
    FText Description;

    /** Everything in this list must be completed to finish the quest. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta=(TitleProperty="Description"))
    TArray<FGrandCityQuestObjective> Objectives;

    /**
     * On: objectives unlock one by one from the top of the list to the bottom.
     * Off: every objective starts at the same time and can be done in any order.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    bool bCompleteObjectivesInOrder = true;

    /** Master switch for the per-objective timers (bUseTimer on each objective). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Timer")
    bool bUseObjectiveTimers = false;

    /** Optional countdown for the whole quest, started on accept. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Timer")
    bool bUseQuestTimer = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Timer",
        meta=(EditCondition="bUseQuestTimer", ClampMin="1.0", Units="s"))
    float QuestTimeLimitSeconds = 300.0f;
};

/** Replicated runtime state of one objective. */
USTRUCT(BlueprintType)
struct GRANDCITYMOBILE_API FGrandCityQuestObjectiveProgress
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    EGrandCityQuestObjectiveStatus Status = EGrandCityQuestObjectiveStatus::Locked;

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    int32 Count = 0;

    UPROPERTY(BlueprintReadOnly, Category="Quest")
    int32 RequiredCount = 1;

    /** One entry per objective target (Deliver: per delivery leg); non-zero once it is done. */
    UPROPERTY()
    TArray<uint8> TargetsDone;

    /** Server world time the objective times out at; negative when it has no timer. */
    UPROPERTY(BlueprintReadOnly, Category="Quest")
    double EndTime = -1.0;

    /** Deliver only: where the cargo on the vehicle was loaded; null while nothing is loaded. */
    UPROPERTY(BlueprintReadOnly, Category="Quest")
    TObjectPtr<AGrandCityQuestTarget> CargoPickup;

    /** Deliver only: the vehicle the cargo was loaded onto at the pickup. */
    UPROPERTY(BlueprintReadOnly, Category="Quest")
    TObjectPtr<AGrandCityVehicle> CargoVehicle;
};
