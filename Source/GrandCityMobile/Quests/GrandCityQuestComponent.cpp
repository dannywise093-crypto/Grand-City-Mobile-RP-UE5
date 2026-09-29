#include "Quests/GrandCityQuestComponent.h"

#include "Quests/GrandCityQuestCargo.h"
#include "Quests/GrandCityQuestGiver.h"
#include "Quests/GrandCityQuestTarget.h"
#include "UI/GrandCityQuestOfferWidget.h"
#include "UI/GrandCityQuestTrackerWidget.h"
#include "GrandCityMobileCharacter.h"
#include "Vehicles/GrandCityVehicle.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGrandCityQuest, Log, All);

namespace GrandCityQuest
{
    /** Slack for latency between the client's range checks and the server's. */
    constexpr float ServerRangeTolerance = 150.0f;
    constexpr float QuestTickInterval = 0.1f;
}

UGrandCityQuestComponent::UGrandCityQuestComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UGrandCityQuestComponent::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocallyControlled())
    {
        TrackerWidget = CreateWidget<UGrandCityQuestTrackerWidget>(
            GetOwningPlayerController(), UGrandCityQuestTrackerWidget::StaticClass());
        if (TrackerWidget)
        {
            TrackerWidget->SetQuestComponent(this);
            TrackerWidget->AddToViewport(5);
        }
        RefreshLocalVisuals();
    }
}

void UGrandCityQuestComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(QuestTickTimer);
    }
    CloseOffer();
    if (TrackerWidget)
    {
        TrackerWidget->RemoveFromParent();
        TrackerWidget = nullptr;
    }
    for (const auto& Pair : CargoActors)
    {
        if (AGrandCityQuestCargo* Cargo = Pair.Value.Get())
        {
            Cargo->Destroy();
        }
    }
    CargoActors.Reset();

    Super::EndPlay(EndPlayReason);
}

void UGrandCityQuestComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UGrandCityQuestComponent, ActiveQuest, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UGrandCityQuestComponent, ObjectiveProgress, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UGrandCityQuestComponent, QuestEndTime, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UGrandCityQuestComponent, CompletedQuestIds, COND_OwnerOnly);
}

// --- Lookup helpers ---------------------------------------------------------------------------

UGrandCityQuestComponent* UGrandCityQuestComponent::FindForActor(const AActor* Actor)
{
    if (!Actor)
    {
        return nullptr;
    }

    const AController* Controller = Cast<AController>(Actor);
    if (!Controller)
    {
        if (const APlayerState* PlayerState = Cast<APlayerState>(Actor))
        {
            Controller = PlayerState->GetOwningController();
        }
        else if (const AGrandCityMobileCharacter* Character = Cast<AGrandCityMobileCharacter>(Actor);
            Character && Character->GetOccupiedVehicle())
        {
            // While driving the controller possesses the vehicle, not the character.
            Controller = Character->GetOccupiedVehicle()->GetController();
        }
        else if (const APawn* Pawn = Cast<APawn>(Actor))
        {
            Controller = Pawn->GetController();
        }
        else
        {
            Controller = Actor->GetInstigatorController();
        }
    }

    return Controller ? Controller->FindComponentByClass<UGrandCityQuestComponent>() : nullptr;
}

void UGrandCityQuestComponent::RefreshLocalQuestVisuals(const UObject* WorldContextObject)
{
    const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World)
    {
        return;
    }

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        const APlayerController* PlayerController = It->Get();
        if (PlayerController && PlayerController->IsLocalController())
        {
            if (UGrandCityQuestComponent* QuestComponent = PlayerController->FindComponentByClass<UGrandCityQuestComponent>())
            {
                QuestComponent->RefreshLocalVisuals();
            }
        }
    }
}

APlayerController* UGrandCityQuestComponent::GetOwningPlayerController() const
{
    return Cast<APlayerController>(GetOwner());
}

bool UGrandCityQuestComponent::IsLocallyControlled() const
{
    const APlayerController* PlayerController = GetOwningPlayerController();
    return PlayerController && PlayerController->IsLocalController();
}

double UGrandCityQuestComponent::GetServerTime() const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return 0.0;
    }
    if (const AGameStateBase* GameState = World->GetGameState())
    {
        return GameState->GetServerWorldTimeSeconds();
    }
    return World->GetTimeSeconds();
}

// --- Objective rules --------------------------------------------------------------------------

bool UGrandCityQuestComponent::IsOrderedObjective(const FGrandCityQuestObjective& Objective)
{
    // Deliver runs its own route logic (GetOpenDeliveryTargets).
    return Objective.Type == EGrandCityQuestObjectiveType::ReachLocation && Objective.bVisitTargetsInOrder;
}

void UGrandCityQuestComponent::GetDeliveryLegs(const FGrandCityQuestObjective& Objective, TArray<FGrandCityDeliveryLeg>& OutLegs)
{
    OutLegs.Reset();
    if (Objective.DeliveryRoute == EGrandCityDeliveryRoute::PickupDropPairs)
    {
        for (const FGrandCityDeliveryLeg& Leg : Objective.DeliveryLegs)
        {
            if (Leg.Pickup && Leg.DropOff)
            {
                OutLegs.Add(Leg);
            }
        }
        return;
    }

    // Multi-drop and return-to-depot: Targets[0] is the pickup of every other target.
    AGrandCityQuestTarget* Pickup = Objective.Targets.IsEmpty() ? nullptr : Objective.Targets[0].Get();
    for (int32 TargetIndex = 1; Pickup && TargetIndex < Objective.Targets.Num(); ++TargetIndex)
    {
        if (Objective.Targets[TargetIndex])
        {
            FGrandCityDeliveryLeg& Leg = OutLegs.AddDefaulted_GetRef();
            Leg.Pickup = Pickup;
            Leg.DropOff = Objective.Targets[TargetIndex];
        }
    }
}

int32 UGrandCityQuestComponent::ComputeRequiredCount(const FGrandCityQuestObjective& Objective)
{
    if (Objective.Type == EGrandCityQuestObjectiveType::Custom)
    {
        return FMath::Max(Objective.RequiredCount, 1);
    }

    if (Objective.Type == EGrandCityQuestObjectiveType::Deliver)
    {
        TArray<FGrandCityDeliveryLeg> Legs;
        GetDeliveryLegs(Objective, Legs);
        return Objective.RequiredCount <= 0 ? Legs.Num() : FMath::Min(Objective.RequiredCount, Legs.Num());
    }

    int32 ValidTargets = 0;
    for (const AGrandCityQuestTarget* Target : Objective.Targets)
    {
        ValidTargets += Target ? 1 : 0;
    }

    // Ordered routes must be driven to the end; the other types may need only some targets.
    if (IsOrderedObjective(Objective) || Objective.RequiredCount <= 0)
    {
        return ValidTargets;
    }
    return FMath::Min(Objective.RequiredCount, ValidTargets);
}

bool UGrandCityQuestComponent::IsTargetOpen(int32 ObjectiveIndex, int32 TargetIndex) const
{
    if (!ActiveQuest || !ObjectiveProgress.IsValidIndex(ObjectiveIndex))
    {
        return false;
    }

    const TArray<FGrandCityQuestObjective>& Objectives = ActiveQuest->GetQuest().Objectives;
    if (!Objectives.IsValidIndex(ObjectiveIndex))
    {
        return false;
    }

    const FGrandCityQuestObjective& Objective = Objectives[ObjectiveIndex];
    const FGrandCityQuestObjectiveProgress& Progress = ObjectiveProgress[ObjectiveIndex];
    // Deliver progress is tracked per leg, not per target; see GetOpenDeliveryTargets.
    if (Objective.Type == EGrandCityQuestObjectiveType::Deliver
        || Progress.Status != EGrandCityQuestObjectiveStatus::Active
        || !Objective.Targets.IsValidIndex(TargetIndex)
        || !Objective.Targets[TargetIndex]
        || (Progress.TargetsDone.IsValidIndex(TargetIndex) && Progress.TargetsDone[TargetIndex] != 0))
    {
        return false;
    }

    if (IsOrderedObjective(Objective))
    {
        const AGrandCityQuestTarget* Next = GetNextOrderedTarget(ObjectiveIndex);
        return Next == Objective.Targets[TargetIndex];
    }
    return true;
}

AGrandCityQuestTarget* UGrandCityQuestComponent::GetNextOrderedTarget(int32 ObjectiveIndex) const
{
    if (!ActiveQuest || !ObjectiveProgress.IsValidIndex(ObjectiveIndex)
        || !ActiveQuest->GetQuest().Objectives.IsValidIndex(ObjectiveIndex))
    {
        return nullptr;
    }

    const FGrandCityQuestObjective& Objective = ActiveQuest->GetQuest().Objectives[ObjectiveIndex];
    const FGrandCityQuestObjectiveProgress& Progress = ObjectiveProgress[ObjectiveIndex];
    if (!IsOrderedObjective(Objective))
    {
        return nullptr;
    }

    for (int32 TargetIndex = 0; TargetIndex < Objective.Targets.Num(); ++TargetIndex)
    {
        const bool bDone = Progress.TargetsDone.IsValidIndex(TargetIndex) && Progress.TargetsDone[TargetIndex] != 0;
        if (Objective.Targets[TargetIndex] && !bDone)
        {
            return Objective.Targets[TargetIndex];
        }
    }
    return nullptr;
}

void UGrandCityQuestComponent::GetOpenDeliveryTargets(
    int32 ObjectiveIndex, TArray<AGrandCityQuestTarget*>& OutTargets, bool& bOutPickup) const
{
    OutTargets.Reset();
    bOutPickup = false;
    if (!ActiveQuest || !ObjectiveProgress.IsValidIndex(ObjectiveIndex)
        || !ActiveQuest->GetQuest().Objectives.IsValidIndex(ObjectiveIndex))
    {
        return;
    }

    const FGrandCityQuestObjective& Objective = ActiveQuest->GetQuest().Objectives[ObjectiveIndex];
    const FGrandCityQuestObjectiveProgress& Progress = ObjectiveProgress[ObjectiveIndex];
    if (Objective.Type != EGrandCityQuestObjectiveType::Deliver || Progress.Status != EGrandCityQuestObjectiveStatus::Active)
    {
        return;
    }

    TArray<FGrandCityDeliveryLeg> Legs;
    GetDeliveryLegs(Objective, Legs);
    bOutPickup = !Progress.CargoPickup;
    for (int32 LegIndex = 0; LegIndex < Legs.Num(); ++LegIndex)
    {
        const bool bDone = Progress.TargetsDone.IsValidIndex(LegIndex) && Progress.TargetsDone[LegIndex] != 0;
        // Loaded cargo can only go to legs that start where it was picked up.
        if (bDone || (!bOutPickup && Legs[LegIndex].Pickup != Progress.CargoPickup))
        {
            continue;
        }

        OutTargets.AddUnique(bOutPickup ? Legs[LegIndex].Pickup.Get() : Legs[LegIndex].DropOff.Get());
        if (Objective.bVisitTargetsInOrder)
        {
            break;
        }
    }
}

bool UGrandCityQuestComponent::IsPawnInTargetRange(const APawn* Pawn, const AGrandCityQuestTarget* Target, float Tolerance) const
{
    if (!Pawn || !Target)
    {
        return false;
    }

    const FVector TargetLocation = Target->GetActorLocation();
    const float Range = Target->GetRadius() + Tolerance;
    if (const AGrandCityVehicle* Vehicle = Cast<AGrandCityVehicle>(Pawn))
    {
        // Measure to the car body so long vehicles do not need to park dead-centre.
        return Vehicle->GetDistanceToVehicle(TargetLocation) <= Range;
    }
    return FVector::DistSquared(Pawn->GetActorLocation(), TargetLocation) <= FMath::Square(Range);
}

float UGrandCityQuestComponent::GetRemainingQuestTime() const
{
    return (ActiveQuest && QuestEndTime >= 0.0)
        ? static_cast<float>(FMath::Max(QuestEndTime - GetServerTime(), 0.0))
        : -1.0f;
}

float UGrandCityQuestComponent::GetRemainingObjectiveTime(int32 ObjectiveIndex) const
{
    if (!ActiveQuest || !ObjectiveProgress.IsValidIndex(ObjectiveIndex))
    {
        return -1.0f;
    }

    const FGrandCityQuestObjectiveProgress& Progress = ObjectiveProgress[ObjectiveIndex];
    if (Progress.Status != EGrandCityQuestObjectiveStatus::Active || Progress.EndTime < 0.0)
    {
        return -1.0f;
    }
    return static_cast<float>(FMath::Max(Progress.EndTime - GetServerTime(), 0.0));
}

// --- Server flow ------------------------------------------------------------------------------

void UGrandCityQuestComponent::ServerAcceptQuest_Implementation(AGrandCityQuestGiver* QuestGiver)
{
    if (!QuestGiver || ActiveQuest || !QuestGiver->CanBeAcceptedBy(CompletedQuestIds))
    {
        return;
    }

    const APlayerController* PlayerController = GetOwningPlayerController();
    const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    const bool bRemoteOffer = PendingRemoteOffer.Get() == QuestGiver;
    const bool bInArea = Pawn && QuestGiver->IsLocationInTriggerArea(
        Pawn->GetActorLocation(), GrandCityQuest::ServerRangeTolerance);
    if (!bRemoteOffer && !bInArea)
    {
        return;
    }

    StartQuest(QuestGiver);
}

void UGrandCityQuestComponent::StartQuest(AGrandCityQuestGiver* QuestGiver)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !QuestGiver)
    {
        return;
    }

    if (ActiveQuest)
    {
        ClearActiveQuest();
    }

    const FGrandCityQuestDefinition& Quest = QuestGiver->GetQuest();
    const double Now = GetServerTime();
    PendingRemoteOffer.Reset();
    ActiveQuest = QuestGiver;
    QuestEndTime = Quest.bUseQuestTimer ? Now + Quest.QuestTimeLimitSeconds : -1.0;

    ObjectiveProgress.SetNum(Quest.Objectives.Num());
    for (int32 Index = 0; Index < Quest.Objectives.Num(); ++Index)
    {
        FGrandCityQuestObjectiveProgress& Progress = ObjectiveProgress[Index];
        Progress = FGrandCityQuestObjectiveProgress();
        int32 DoneSlots = Quest.Objectives[Index].Targets.Num();
        if (Quest.Objectives[Index].Type == EGrandCityQuestObjectiveType::Deliver)
        {
            TArray<FGrandCityDeliveryLeg> Legs;
            GetDeliveryLegs(Quest.Objectives[Index], Legs);
            DoneSlots = Legs.Num();
        }
        Progress.TargetsDone.Init(0, DoneSlots);
        Progress.RequiredCount = ComputeRequiredCount(Quest.Objectives[Index]);
    }

    // Every attempt starts from a clean world state.
    RespawnQuestTargets(QuestGiver);

    GetWorld()->GetTimerManager().SetTimer(
        QuestTickTimer, this, &UGrandCityQuestComponent::TickQuest, GrandCityQuest::QuestTickInterval, true);

    UE_LOG(LogGrandCityQuest, Log, TEXT("%s started quest %s."),
        *GetNameSafe(GetOwner()), *QuestGiver->GetQuestId().ToString());
    BroadcastQuestEvent(QuestGiver, EGrandCityQuestEvent::Started, Quest.Title);

    if (Quest.Objectives.IsEmpty())
    {
        CompleteQuest();
        return;
    }

    if (Quest.bCompleteObjectivesInOrder)
    {
        ActivateObjective(0);
    }
    else
    {
        for (int32 Index = 0; Index < Quest.Objectives.Num() && ActiveQuest == QuestGiver; ++Index)
        {
            ActivateObjective(Index);
        }
    }

    OnRep_QuestState();
}

void UGrandCityQuestComponent::ActivateObjective(int32 ObjectiveIndex)
{
    if (!ActiveQuest || !ObjectiveProgress.IsValidIndex(ObjectiveIndex))
    {
        return;
    }

    const FGrandCityQuestDefinition& Quest = ActiveQuest->GetQuest();
    const FGrandCityQuestObjective& Objective = Quest.Objectives[ObjectiveIndex];
    FGrandCityQuestObjectiveProgress& Progress = ObjectiveProgress[ObjectiveIndex];
    Progress.Status = EGrandCityQuestObjectiveStatus::Active;
    Progress.EndTime = (Quest.bUseObjectiveTimers && Objective.bUseTimer)
        ? GetServerTime() + Objective.TimeLimitSeconds
        : -1.0;

    if (Progress.Count >= Progress.RequiredCount)
    {
        if (Objective.Type != EGrandCityQuestObjectiveType::Custom)
        {
            UE_LOG(LogGrandCityQuest, Warning,
                TEXT("Quest %s objective %d has no targets assigned; completing it automatically."),
                *ActiveQuest->GetQuestId().ToString(), ObjectiveIndex);
        }
        CompleteObjective(ObjectiveIndex);
    }
}

void UGrandCityQuestComponent::TickQuest()
{
    AGrandCityQuestGiver* Quest = ActiveQuest;
    if (!Quest)
    {
        GetWorld()->GetTimerManager().ClearTimer(QuestTickTimer);
        return;
    }

    const double Now = GetServerTime();
    if (QuestEndTime >= 0.0 && Now >= QuestEndTime)
    {
        FailQuest(NSLOCTEXT("GrandCityQuest", "QuestTimeUp", "Time is up."));
        return;
    }

    const TArray<FGrandCityQuestObjective>& Objectives = Quest->GetQuest().Objectives;
    for (int32 ObjectiveIndex = 0; ObjectiveIndex < ObjectiveProgress.Num(); ++ObjectiveIndex)
    {
        const FGrandCityQuestObjectiveProgress& Progress = ObjectiveProgress[ObjectiveIndex];
        if (Progress.Status == EGrandCityQuestObjectiveStatus::Active && Progress.EndTime >= 0.0 && Now >= Progress.EndTime)
        {
            FailQuest(FText::Format(
                NSLOCTEXT("GrandCityQuest", "ObjectiveTimeUp", "Ran out of time: {0}"),
                Objectives[ObjectiveIndex].Description));
            return;
        }

        // Loaded cargo only travels on its vehicle; without it the delivery cannot finish.
        if (Objectives[ObjectiveIndex].Type == EGrandCityQuestObjectiveType::Deliver
            && Progress.Status == EGrandCityQuestObjectiveStatus::Active
            && Progress.CargoPickup && !IsValid(Progress.CargoVehicle))
        {
            FailQuest(NSLOCTEXT("GrandCityQuest", "CargoLost", "The cargo was lost."));
            return;
        }
    }

    const APlayerController* PlayerController = GetOwningPlayerController();
    APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!Pawn)
    {
        return;
    }

    AGrandCityVehicle* Vehicle = Cast<AGrandCityVehicle>(Pawn);
    for (int32 ObjectiveIndex = 0; ObjectiveIndex < Objectives.Num(); ++ObjectiveIndex)
    {
        const FGrandCityQuestObjective& Objective = Objectives[ObjectiveIndex];
        if (Objective.Type == EGrandCityQuestObjectiveType::Deliver)
        {
            TickDelivery(ObjectiveIndex, Vehicle);
            if (ActiveQuest != Quest)
            {
                return;
            }
            continue;
        }

        for (int32 TargetIndex = 0; TargetIndex < Objective.Targets.Num(); ++TargetIndex)
        {
            if (!IsTargetOpen(ObjectiveIndex, TargetIndex))
            {
                continue;
            }

            AGrandCityQuestTarget* Target = Objective.Targets[TargetIndex];
            if (!IsPawnInTargetRange(Pawn, Target))
            {
                continue;
            }

            switch (Objective.Type)
            {
            case EGrandCityQuestObjectiveType::ReachLocation:
            case EGrandCityQuestObjectiveType::Collect:
                if (!Objective.bRequireVehicle || Vehicle)
                {
                    CompleteTarget(ObjectiveIndex, TargetIndex);
                }
                break;
            case EGrandCityQuestObjectiveType::Destroy:
                if (Vehicle && Target->bDestroyByVehicleRam && !Target->IsDestroyed()
                    && FMath::Abs(Vehicle->GetForwardSpeed()) >= Target->MinRamSpeed
                    && Target->ApplyQuestDamage(Target->MaxHealth))
                {
                    CreditDestroyedTarget(Target);
                }
                break;
            default:
                break;
            }

            // Completing a target can finish or fail the quest and reset the arrays.
            if (ActiveQuest != Quest)
            {
                return;
            }
        }
    }
}

void UGrandCityQuestComponent::TickDelivery(int32 ObjectiveIndex, AGrandCityVehicle* Vehicle)
{
    // Cargo always rides on a vehicle, so on foot nothing can be picked up or dropped off.
    if (!Vehicle || !ObjectiveProgress.IsValidIndex(ObjectiveIndex))
    {
        return;
    }

    TArray<AGrandCityQuestTarget*> OpenTargets;
    bool bPickup = false;
    GetOpenDeliveryTargets(ObjectiveIndex, OpenTargets, bPickup);

    FGrandCityQuestObjectiveProgress& Progress = ObjectiveProgress[ObjectiveIndex];
    if (!bPickup && Progress.CargoVehicle != Vehicle)
    {
        return;
    }

    for (AGrandCityQuestTarget* Target : OpenTargets)
    {
        if (!IsPawnInTargetRange(Vehicle, Target))
        {
            continue;
        }

        if (bPickup)
        {
            Progress.CargoPickup = Target;
            Progress.CargoVehicle = Vehicle;
            OnRep_QuestState();
        }
        else
        {
            CompleteDelivery(ObjectiveIndex, Target);
        }
        // One step per tick, so a drop-off that is also the next pickup reloads on the following tick.
        return;
    }
}

void UGrandCityQuestComponent::CompleteDelivery(int32 ObjectiveIndex, const AGrandCityQuestTarget* DropOff)
{
    const FGrandCityQuestObjective& Objective = ActiveQuest->GetQuest().Objectives[ObjectiveIndex];
    FGrandCityQuestObjectiveProgress& Progress = ObjectiveProgress[ObjectiveIndex];

    TArray<FGrandCityDeliveryLeg> Legs;
    GetDeliveryLegs(Objective, Legs);
    for (int32 LegIndex = 0; LegIndex < Legs.Num(); ++LegIndex)
    {
        const bool bDone = Progress.TargetsDone.IsValidIndex(LegIndex) && Progress.TargetsDone[LegIndex] != 0;
        if (bDone || Legs[LegIndex].Pickup != Progress.CargoPickup || Legs[LegIndex].DropOff != DropOff)
        {
            continue;
        }

        Progress.TargetsDone[LegIndex] = 1;
        // Multi-drop keeps its single load for the next stop; the other routes use it up here.
        if (Objective.DeliveryRoute != EGrandCityDeliveryRoute::MultiDrop)
        {
            Progress.CargoPickup = nullptr;
            Progress.CargoVehicle = nullptr;
        }
        AddObjectiveProgress(ObjectiveIndex, 1);
        return;
    }
}

void UGrandCityQuestComponent::CompleteTarget(int32 ObjectiveIndex, int32 TargetIndex)
{
    if (!ObjectiveProgress.IsValidIndex(ObjectiveIndex))
    {
        return;
    }

    FGrandCityQuestObjectiveProgress& Progress = ObjectiveProgress[ObjectiveIndex];
    if (!Progress.TargetsDone.IsValidIndex(TargetIndex) || Progress.TargetsDone[TargetIndex] != 0)
    {
        return;
    }

    Progress.TargetsDone[TargetIndex] = 1;
    AddObjectiveProgress(ObjectiveIndex, 1);
}

void UGrandCityQuestComponent::AddObjectiveProgress(int32 ObjectiveIndex, int32 Amount)
{
    AGrandCityQuestGiver* Quest = ActiveQuest;
    FGrandCityQuestObjectiveProgress& Progress = ObjectiveProgress[ObjectiveIndex];
    Progress.Count = FMath::Min(Progress.Count + Amount, Progress.RequiredCount);
    if (Progress.Count >= Progress.RequiredCount)
    {
        CompleteObjective(ObjectiveIndex);
    }

    if (ActiveQuest == Quest)
    {
        OnRep_QuestState();
    }
}

void UGrandCityQuestComponent::CompleteObjective(int32 ObjectiveIndex)
{
    AGrandCityQuestGiver* Quest = ActiveQuest;
    if (!Quest || !ObjectiveProgress.IsValidIndex(ObjectiveIndex)
        || ObjectiveProgress[ObjectiveIndex].Status == EGrandCityQuestObjectiveStatus::Completed)
    {
        return;
    }

    FGrandCityQuestObjectiveProgress& Progress = ObjectiveProgress[ObjectiveIndex];
    Progress.Status = EGrandCityQuestObjectiveStatus::Completed;
    Progress.EndTime = -1.0;
    Progress.CargoPickup = nullptr;
    Progress.CargoVehicle = nullptr;

    bool bAllCompleted = true;
    for (const FGrandCityQuestObjectiveProgress& Other : ObjectiveProgress)
    {
        bAllCompleted &= Other.Status == EGrandCityQuestObjectiveStatus::Completed;
    }
    if (bAllCompleted)
    {
        CompleteQuest();
        return;
    }

    BroadcastQuestEvent(Quest, EGrandCityQuestEvent::ObjectiveCompleted,
        Quest->GetQuest().Objectives[ObjectiveIndex].Description);

    if (Quest->GetQuest().bCompleteObjectivesInOrder && ObjectiveProgress.IsValidIndex(ObjectiveIndex + 1))
    {
        ActivateObjective(ObjectiveIndex + 1);
    }
}

void UGrandCityQuestComponent::CompleteQuest()
{
    AGrandCityQuestGiver* Quest = ActiveQuest;
    if (!Quest)
    {
        return;
    }

    CompletedQuestIds.AddUnique(Quest->GetQuestId());
    ClearActiveQuest();
    UE_LOG(LogGrandCityQuest, Log, TEXT("%s completed quest %s."),
        *GetNameSafe(GetOwner()), *Quest->GetQuestId().ToString());
    BroadcastQuestEvent(Quest, EGrandCityQuestEvent::Completed, Quest->GetQuest().Title);

    // A minigame never chains on; the player walks back to its giver for another round.
    AGrandCityQuestGiver* NextQuest = Quest->bIsMinigame ? nullptr : Quest->NextQuest.Get();
    if (NextQuest && Quest->bOfferNextQuestImmediately && NextQuest->CanBeAcceptedBy(CompletedQuestIds))
    {
        PendingRemoteOffer = NextQuest;
        ClientOfferQuest(NextQuest);
    }
}

void UGrandCityQuestComponent::FailQuest(FText Reason)
{
    AGrandCityQuestGiver* Quest = ActiveQuest;
    if (!Quest || !GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    ClearActiveQuest();
    RespawnQuestTargets(Quest);
    UE_LOG(LogGrandCityQuest, Log, TEXT("%s failed quest %s: %s"),
        *GetNameSafe(GetOwner()), *Quest->GetQuestId().ToString(), *Reason.ToString());
    BroadcastQuestEvent(Quest, EGrandCityQuestEvent::Failed, Reason);

    if (Quest->bAutoRestartOnFail)
    {
        StartQuest(Quest);
    }
}

void UGrandCityQuestComponent::ClearActiveQuest()
{
    GetWorld()->GetTimerManager().ClearTimer(QuestTickTimer);
    ActiveQuest = nullptr;
    ObjectiveProgress.Reset();
    QuestEndTime = -1.0;
    OnRep_QuestState();
}

void UGrandCityQuestComponent::RespawnQuestTargets(const AGrandCityQuestGiver* QuestGiver)
{
    if (!QuestGiver)
    {
        return;
    }

    for (const FGrandCityQuestObjective& Objective : QuestGiver->GetQuest().Objectives)
    {
        if (Objective.Type != EGrandCityQuestObjectiveType::Destroy)
        {
            continue;
        }
        for (AGrandCityQuestTarget* Target : Objective.Targets)
        {
            if (Target)
            {
                Target->Respawn();
            }
        }
    }
}

void UGrandCityQuestComponent::ReportQuestEvent(FName EventTag, int32 Amount)
{
    AGrandCityQuestGiver* Quest = ActiveQuest;
    if (!Quest || EventTag.IsNone() || Amount <= 0 || !GetOwner()->HasAuthority())
    {
        return;
    }

    const TArray<FGrandCityQuestObjective>& Objectives = Quest->GetQuest().Objectives;
    for (int32 ObjectiveIndex = 0; ObjectiveIndex < ObjectiveProgress.Num(); ++ObjectiveIndex)
    {
        if (Objectives[ObjectiveIndex].Type == EGrandCityQuestObjectiveType::Custom
            && Objectives[ObjectiveIndex].CustomEventTag == EventTag
            && ObjectiveProgress[ObjectiveIndex].Status == EGrandCityQuestObjectiveStatus::Active)
        {
            AddObjectiveProgress(ObjectiveIndex, Amount);
            if (ActiveQuest != Quest)
            {
                return;
            }
        }
    }
}

void UGrandCityQuestComponent::ReportQuestEventForPlayer(AActor* PlayerActor, FName EventTag, int32 Amount)
{
    if (UGrandCityQuestComponent* QuestComponent = FindForActor(PlayerActor))
    {
        QuestComponent->ReportQuestEvent(EventTag, Amount);
    }
}

void UGrandCityQuestComponent::HandleTargetDamaged(AGrandCityQuestTarget* Target, float Damage)
{
    if (!ActiveQuest || !Target)
    {
        return;
    }

    const TArray<FGrandCityQuestObjective>& Objectives = ActiveQuest->GetQuest().Objectives;
    for (int32 ObjectiveIndex = 0; ObjectiveIndex < Objectives.Num(); ++ObjectiveIndex)
    {
        if (Objectives[ObjectiveIndex].Type != EGrandCityQuestObjectiveType::Destroy)
        {
            continue;
        }

        const int32 TargetIndex = Objectives[ObjectiveIndex].Targets.IndexOfByKey(Target);
        if (IsTargetOpen(ObjectiveIndex, TargetIndex))
        {
            if (Target->ApplyQuestDamage(Damage))
            {
                CreditDestroyedTarget(Target);
            }
            return;
        }
    }
}

void UGrandCityQuestComponent::CreditDestroyedTarget(AGrandCityQuestTarget* Target)
{
    AGrandCityQuestGiver* Quest = ActiveQuest;
    if (!Quest)
    {
        return;
    }

    // The same target may appear in several Destroy objectives running in parallel.
    const TArray<FGrandCityQuestObjective>& Objectives = Quest->GetQuest().Objectives;
    for (int32 ObjectiveIndex = 0; ObjectiveIndex < Objectives.Num(); ++ObjectiveIndex)
    {
        if (Objectives[ObjectiveIndex].Type != EGrandCityQuestObjectiveType::Destroy)
        {
            continue;
        }

        const int32 TargetIndex = Objectives[ObjectiveIndex].Targets.IndexOfByKey(Target);
        if (IsTargetOpen(ObjectiveIndex, TargetIndex))
        {
            CompleteTarget(ObjectiveIndex, TargetIndex);
            if (ActiveQuest != Quest)
            {
                return;
            }
        }
    }
}

void UGrandCityQuestComponent::ServerInteractWithTarget_Implementation(AGrandCityQuestTarget* Target)
{
    AGrandCityQuestGiver* Quest = ActiveQuest;
    const APlayerController* PlayerController = GetOwningPlayerController();
    const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!Quest || !Target || !IsPawnInTargetRange(Pawn, Target, GrandCityQuest::ServerRangeTolerance))
    {
        return;
    }

    const TArray<FGrandCityQuestObjective>& Objectives = Quest->GetQuest().Objectives;
    for (int32 ObjectiveIndex = 0; ObjectiveIndex < Objectives.Num(); ++ObjectiveIndex)
    {
        const FGrandCityQuestObjective& Objective = Objectives[ObjectiveIndex];
        const int32 TargetIndex = Objective.Targets.IndexOfByKey(Target);
        if (!IsTargetOpen(ObjectiveIndex, TargetIndex))
        {
            continue;
        }

        if (Objective.Type == EGrandCityQuestObjectiveType::Interact)
        {
            CompleteTarget(ObjectiveIndex, TargetIndex);
            return;
        }
        if (Objective.Type == EGrandCityQuestObjectiveType::Destroy && Target->bDamageByInteract)
        {
            if (Target->ApplyQuestDamage(Target->InteractDamage))
            {
                CreditDestroyedTarget(Target);
            }
            return;
        }
    }
}

void UGrandCityQuestComponent::BroadcastQuestEvent(
    AGrandCityQuestGiver* QuestGiver, EGrandCityQuestEvent Event, const FText& Detail)
{
    switch (Event)
    {
    case EGrandCityQuestEvent::Started:
        OnQuestStarted.Broadcast(QuestGiver);
        break;
    case EGrandCityQuestEvent::Completed:
        OnQuestCompleted.Broadcast(QuestGiver);
        break;
    case EGrandCityQuestEvent::Failed:
        OnQuestFailed.Broadcast(QuestGiver, Detail);
        break;
    default:
        break;
    }

    ClientQuestEvent(QuestGiver, Event, Detail);
}

void UGrandCityQuestComponent::ClientQuestEvent_Implementation(
    AGrandCityQuestGiver* QuestGiver, EGrandCityQuestEvent Event, const FText& Detail)
{
    // A listen-server host already broadcast these on the server side.
    if (!GetOwner()->HasAuthority())
    {
        switch (Event)
        {
        case EGrandCityQuestEvent::Started:
            OnQuestStarted.Broadcast(QuestGiver);
            break;
        case EGrandCityQuestEvent::Completed:
            OnQuestCompleted.Broadcast(QuestGiver);
            break;
        case EGrandCityQuestEvent::Failed:
            OnQuestFailed.Broadcast(QuestGiver, Detail);
            break;
        default:
            break;
        }
    }

    if (!TrackerWidget)
    {
        return;
    }

    const bool bMinigame = QuestGiver && QuestGiver->bIsMinigame;
    switch (Event)
    {
    case EGrandCityQuestEvent::Started:
        TrackerWidget->ShowBanner(bMinigame
                ? NSLOCTEXT("GrandCityQuest", "BannerMinigameStarted", "MINIGAME STARTED")
                : NSLOCTEXT("GrandCityQuest", "BannerStarted", "QUEST STARTED"),
            Detail, FLinearColor(1.0f, 0.82f, 0.15f));
        break;
    case EGrandCityQuestEvent::ObjectiveCompleted:
        TrackerWidget->ShowBanner(
            NSLOCTEXT("GrandCityQuest", "BannerObjective", "OBJECTIVE COMPLETE"), Detail, FLinearColor(0.35f, 0.85f, 1.0f));
        break;
    case EGrandCityQuestEvent::Completed:
        TrackerWidget->ShowBanner(bMinigame
                ? NSLOCTEXT("GrandCityQuest", "BannerMinigameCompleted", "MINIGAME COMPLETE")
                : NSLOCTEXT("GrandCityQuest", "BannerCompleted", "QUEST COMPLETE"),
            Detail, FLinearColor(0.3f, 1.0f, 0.4f));
        break;
    case EGrandCityQuestEvent::Failed:
        TrackerWidget->ShowBanner(bMinigame
                ? NSLOCTEXT("GrandCityQuest", "BannerMinigameFailed", "MINIGAME FAILED")
                : NSLOCTEXT("GrandCityQuest", "BannerFailed", "QUEST FAILED"),
            Detail, FLinearColor(1.0f, 0.3f, 0.25f));
        break;
    }
}

void UGrandCityQuestComponent::ClientOfferQuest_Implementation(AGrandCityQuestGiver* QuestGiver)
{
    if (QuestGiver)
    {
        ShowOffer(QuestGiver, true);
    }
}

void UGrandCityQuestComponent::OnRep_QuestState()
{
    if (IsLocallyControlled())
    {
        if (ActiveQuest && OfferWidget)
        {
            CloseOffer();
        }
        RefreshLocalVisuals();
    }
}

// --- Local interaction and UI -----------------------------------------------------------------

void UGrandCityQuestComponent::RefreshLocalVisuals()
{
    UWorld* World = GetWorld();
    if (!World || !IsLocallyControlled())
    {
        return;
    }

    for (TActorIterator<AGrandCityQuestGiver> It(World); It; ++It)
    {
        It->SetLocallyAvailable(!ActiveQuest && It->CanBeAcceptedBy(CompletedQuestIds));
    }

    TSet<const AGrandCityQuestTarget*> OpenTargets;
    if (ActiveQuest)
    {
        const TArray<FGrandCityQuestObjective>& Objectives = ActiveQuest->GetQuest().Objectives;
        TArray<AGrandCityQuestTarget*> DeliveryTargets;
        for (int32 ObjectiveIndex = 0; ObjectiveIndex < Objectives.Num(); ++ObjectiveIndex)
        {
            if (Objectives[ObjectiveIndex].Type == EGrandCityQuestObjectiveType::Deliver)
            {
                bool bPickup = false;
                GetOpenDeliveryTargets(ObjectiveIndex, DeliveryTargets, bPickup);
                OpenTargets.Append(DeliveryTargets);
                continue;
            }

            for (int32 TargetIndex = 0; TargetIndex < Objectives[ObjectiveIndex].Targets.Num(); ++TargetIndex)
            {
                if (IsTargetOpen(ObjectiveIndex, TargetIndex))
                {
                    OpenTargets.Add(Objectives[ObjectiveIndex].Targets[TargetIndex]);
                }
            }
        }
    }

    for (TActorIterator<AGrandCityQuestTarget> It(World); It; ++It)
    {
        It->SetLocalQuestState(OpenTargets.Contains(*It));
    }

    RefreshLocalCargo();
}

void UGrandCityQuestComponent::RefreshLocalCargo()
{
    UWorld* World = GetWorld();
    TMap<TPair<int32, const AGrandCityQuestTarget*>, TWeakObjectPtr<AGrandCityQuestCargo>> OldCargo = MoveTemp(CargoActors);
    CargoActors.Reset();

    // One box per (objective, pickup): it waits on the ground, then the same box hops onto the vehicle.
    auto ShowCargo = [&](int32 ObjectiveIndex, const AGrandCityQuestTarget* Pickup, AGrandCityVehicle* Carrier)
    {
        const TPair<int32, const AGrandCityQuestTarget*> Key(ObjectiveIndex, Pickup);
        TWeakObjectPtr<AGrandCityQuestCargo> Existing;
        OldCargo.RemoveAndCopyValue(Key, Existing);
        AGrandCityQuestCargo* Cargo = Existing.Get();
        const bool bExisted = Cargo != nullptr;
        if (!Cargo)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Cargo = World->SpawnActor<AGrandCityQuestCargo>(SpawnParams);
            if (!Cargo)
            {
                return;
            }
        }

        if (Carrier)
        {
            // Hop over from the pickup; cargo first seen already loaded just appears on the roof.
            Cargo->LoadOnto(Carrier, bExisted);
        }
        else
        {
            Cargo->PlaceAt(Pickup->GetActorLocation(), Pickup->GetActorRotation());
        }
        CargoActors.Add(Key, Cargo);
    };

    const int32 ObjectiveCount = ActiveQuest && World ? ObjectiveProgress.Num() : 0;
    TArray<AGrandCityQuestTarget*> Pickups;
    for (int32 ObjectiveIndex = 0; ObjectiveIndex < ObjectiveCount; ++ObjectiveIndex)
    {
        const FGrandCityQuestObjectiveProgress& Progress = ObjectiveProgress[ObjectiveIndex];
        if (Progress.CargoPickup)
        {
            if (AGrandCityVehicle* Carrier = Progress.CargoVehicle.Get())
            {
                ShowCargo(ObjectiveIndex, Progress.CargoPickup, Carrier);
            }
            continue;
        }

        // Nothing loaded: a box waits at every pickup the player can go to now.
        bool bPickup = false;
        GetOpenDeliveryTargets(ObjectiveIndex, Pickups, bPickup);
        for (const AGrandCityQuestTarget* Pickup : Pickups)
        {
            ShowCargo(ObjectiveIndex, Pickup, nullptr);
        }
    }

    // Delivered, failed or abandoned.
    for (const auto& Pair : OldCargo)
    {
        if (AGrandCityQuestCargo* Cargo = Pair.Value.Get())
        {
            Cargo->Destroy();
        }
    }
}

bool UGrandCityQuestComponent::UpdateLocalInteraction()
{
    NearbyQuestGiver.Reset();
    NearbyInteractTarget.Reset();

    const APlayerController* PlayerController = GetOwningPlayerController();
    const AGrandCityMobileCharacter* Character = PlayerController
        ? Cast<AGrandCityMobileCharacter>(PlayerController->GetPawn())
        : nullptr;
    UWorld* World = GetWorld();

    // Walking out of a giver's area closes its offer window, like stepping away from a car.
    if (OfferWidget && !bOfferIsRemote)
    {
        const AGrandCityQuestGiver* Offered = OfferedQuest.Get();
        if (!Offered || !Character
            || !Offered->IsLocationInTriggerArea(Character->GetActorLocation(), GrandCityQuest::ServerRangeTolerance * 0.5f))
        {
            CloseOffer();
        }
    }

    if (!Character || Character->GetOccupiedVehicle() || !World)
    {
        return false;
    }

    const FVector Location = Character->GetActorLocation();
    if (ActiveQuest)
    {
        float BestDistanceSq = TNumericLimits<float>::Max();
        const TArray<FGrandCityQuestObjective>& Objectives = ActiveQuest->GetQuest().Objectives;
        for (int32 ObjectiveIndex = 0; ObjectiveIndex < Objectives.Num(); ++ObjectiveIndex)
        {
            const FGrandCityQuestObjective& Objective = Objectives[ObjectiveIndex];
            for (int32 TargetIndex = 0; TargetIndex < Objective.Targets.Num(); ++TargetIndex)
            {
                AGrandCityQuestTarget* Target = Objective.Targets[TargetIndex];
                const bool bInteractable = Objective.Type == EGrandCityQuestObjectiveType::Interact
                    || (Objective.Type == EGrandCityQuestObjectiveType::Destroy && Target && Target->bDamageByInteract
                        && !Target->IsDestroyed());
                if (!bInteractable || !IsTargetOpen(ObjectiveIndex, TargetIndex) || !IsPawnInTargetRange(Character, Target))
                {
                    continue;
                }

                const float DistanceSq = FVector::DistSquared(Location, Target->GetActorLocation());
                if (DistanceSq < BestDistanceSq)
                {
                    BestDistanceSq = DistanceSq;
                    NearbyInteractTarget = Target;
                }
            }
        }
    }
    else
    {
        for (TActorIterator<AGrandCityQuestGiver> It(World); It; ++It)
        {
            if (It->CanBeAcceptedBy(CompletedQuestIds) && It->IsLocationInTriggerArea(Location))
            {
                NearbyQuestGiver = *It;
                break;
            }
        }
    }

    return NearbyInteractTarget.IsValid() || NearbyQuestGiver.IsValid();
}

FText UGrandCityQuestComponent::GetLocalInteractionLabel() const
{
    if (const AGrandCityQuestTarget* Target = NearbyInteractTarget.Get())
    {
        if (ActiveQuest)
        {
            for (const FGrandCityQuestObjective& Objective : ActiveQuest->GetQuest().Objectives)
            {
                if (Objective.Type == EGrandCityQuestObjectiveType::Destroy && Objective.Targets.Contains(Target))
                {
                    return NSLOCTEXT("GrandCityQuest", "InteractDestroy", "DESTROY");
                }
            }
        }
        return NSLOCTEXT("GrandCityQuest", "InteractUse", "USE");
    }
    if (const AGrandCityQuestGiver* QuestGiver = NearbyQuestGiver.Get(); QuestGiver && QuestGiver->bIsMinigame)
    {
        return NSLOCTEXT("GrandCityQuest", "InteractMinigame", "MINIGAME");
    }
    return NSLOCTEXT("GrandCityQuest", "InteractQuest", "QUEST");
}

bool UGrandCityQuestComponent::TryLocalInteract()
{
    if (OfferWidget)
    {
        // The window is answered with its own buttons.
        return true;
    }

    UpdateLocalInteraction();
    if (AGrandCityQuestTarget* Target = NearbyInteractTarget.Get())
    {
        ServerInteractWithTarget(Target);
        return true;
    }
    if (AGrandCityQuestGiver* QuestGiver = NearbyQuestGiver.Get())
    {
        ShowOffer(QuestGiver, false);
        return true;
    }
    return false;
}

void UGrandCityQuestComponent::ShowOffer(AGrandCityQuestGiver* QuestGiver, bool bRemoteOffer)
{
    APlayerController* PlayerController = GetOwningPlayerController();
    // A follow-up offer RPC can arrive before the replicated "quest finished" state does.
    if (!PlayerController || !QuestGiver || (ActiveQuest && !bRemoteOffer))
    {
        return;
    }

    if (!OfferWidget)
    {
        OfferWidget = CreateWidget<UGrandCityQuestOfferWidget>(PlayerController, UGrandCityQuestOfferWidget::StaticClass());
        if (!OfferWidget)
        {
            return;
        }
        OfferWidget->OnAccepted.BindUObject(this, &UGrandCityQuestComponent::HandleOfferAccepted);
        OfferWidget->OnDeclined.BindUObject(this, &UGrandCityQuestComponent::HandleOfferDeclined);
    }

    OfferedQuest = QuestGiver;
    bOfferIsRemote = bRemoteOffer;
    OfferWidget->SetQuest(QuestGiver->GetQuest());
    if (!OfferWidget->IsInViewport())
    {
        OfferWidget->AddToViewport(20);
    }

    // Desktop needs a cursor to click the buttons; touch screens already work. Checked by
    // platform, not by the touch interface: bAlwaysShowTouchInterface keeps the joystick on
    // in PIE, where the mouse is still the only pointer.
    if (PLATFORM_DESKTOP)
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PlayerController->SetInputMode(InputMode);
        PlayerController->SetShowMouseCursor(true);
        bOfferChangedInputMode = true;
    }
}

void UGrandCityQuestComponent::CloseOffer()
{
    if (OfferWidget)
    {
        OfferWidget->RemoveFromParent();
        OfferWidget = nullptr;
    }
    OfferedQuest.Reset();
    bOfferIsRemote = false;

    if (bOfferChangedInputMode)
    {
        bOfferChangedInputMode = false;
        if (APlayerController* PlayerController = GetOwningPlayerController())
        {
            PlayerController->SetInputMode(FInputModeGameOnly());
            PlayerController->SetShowMouseCursor(false);
        }
    }
}

void UGrandCityQuestComponent::HandleOfferAccepted()
{
    if (AGrandCityQuestGiver* QuestGiver = OfferedQuest.Get())
    {
        ServerAcceptQuest(QuestGiver);
    }
    CloseOffer();
}

void UGrandCityQuestComponent::HandleOfferDeclined()
{
    CloseOffer();
}

bool UGrandCityQuestComponent::AcceptLocalOffer()
{
    if (!OfferWidget)
    {
        return false;
    }
    HandleOfferAccepted();
    return true;
}

bool UGrandCityQuestComponent::DeclineLocalOffer()
{
    if (!OfferWidget)
    {
        return false;
    }
    HandleOfferDeclined();
    return true;
}
