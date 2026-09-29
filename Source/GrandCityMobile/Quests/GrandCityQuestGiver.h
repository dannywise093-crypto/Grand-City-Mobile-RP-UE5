#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Quests/GrandCityQuestTypes.h"
#include "GrandCityQuestGiver.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Place in the level to offer a quest. A player standing inside TriggerArea can interact
 * (E key / QUEST button) to open the offer window. Move/scale the actor and TriggerArea to
 * change where the quest is picked up; the quest itself is edited in the Details panel.
 */
UCLASS()
class GRANDCITYMOBILE_API AGrandCityQuestGiver : public AActor
{
    GENERATED_BODY()

public:
    AGrandCityQuestGiver();

    const FGrandCityQuestDefinition& GetQuest() const { return Quest; }
    FName GetQuestId() const;

    /** True when WorldLocation lies inside the trigger box, grown by Tolerance on every side. */
    bool IsLocationInTriggerArea(const FVector& WorldLocation, float Tolerance = 0.0f) const;

    /** Every prerequisite quest is in CompletedQuestIds. */
    bool ArePrerequisitesMet(const TArray<FName>& CompletedQuestIds) const;

    /** Can a player with this history accept the quest right now (ignores location). */
    bool CanBeAcceptedBy(const TArray<FName>& CompletedQuestIds) const;

    /** Local only. Shows or hides the marker for the local player. */
    void SetLocallyAvailable(bool bAvailable);

    /**
     * Off (quest): done once and can lead straight into NextQuest.
     * On (minigame): can be played again from the start. After finishing, the player has to
     * come back to this giver to accept or decline another round.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Flow")
    bool bIsMinigame = false;

    /** Quests that must be completed before this one is offered. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Flow")
    TArray<TObjectPtr<AGrandCityQuestGiver>> RequiredQuests;

    /** Quest that continues the story. It becomes available once this one is completed. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Flow", meta=(EditCondition="!bIsMinigame", EditConditionHides))
    TObjectPtr<AGrandCityQuestGiver> NextQuest;

    /** Open NextQuest's offer window right after completing this quest, wherever the player is. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Flow",
        meta=(EditCondition="!bIsMinigame && NextQuest != nullptr", EditConditionHides))
    bool bOfferNextQuestImmediately = true;

    /**
     * On fail the quest always resets to the beginning. On: it restarts immediately.
     * Off: the player has to come back to this giver and accept it again.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Flow")
    bool bAutoRestartOnFail = false;

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest")
    TObjectPtr<USceneComponent> Root;

    /** Players standing inside this box can interact with the quest. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest")
    TObjectPtr<UBoxComponent> TriggerArea;

    /** Visual marker; replace the mesh/material freely. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest")
    TObjectPtr<UStaticMeshComponent> Marker;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest")
    TObjectPtr<UTextRenderComponent> Label;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta=(ShowOnlyInnerProperties))
    FGrandCityQuestDefinition Quest;
};
