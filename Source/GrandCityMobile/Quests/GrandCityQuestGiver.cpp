#include "Quests/GrandCityQuestGiver.h"

#include "Quests/GrandCityQuestComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AGrandCityQuestGiver::AGrandCityQuestGiver()
{
    PrimaryActorTick.bCanEverTick = false;
    // Placed in the level on every machine; quest state lives on the player controllers.
    bReplicates = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    TriggerArea = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerArea"));
    TriggerArea->SetupAttachment(Root);
    TriggerArea->SetBoxExtent(FVector(200.0f, 200.0f, 120.0f));
    TriggerArea->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
    // Area checks are geometric (IsLocationInTriggerArea), so the box never collides.
    TriggerArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TriggerArea->SetGenerateOverlapEvents(false);
    TriggerArea->ShapeColor = FColor(255, 200, 0);

    Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
    Marker->SetupAttachment(Root);
    Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Marker->SetRelativeLocation(FVector(0.0f, 0.0f, 5.0f));
    Marker->SetRelativeScale3D(FVector(1.5f, 1.5f, 0.05f));
    Marker->CastShadow = false;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylinderMesh.Succeeded())
    {
        Marker->SetStaticMesh(CylinderMesh.Object);
    }

    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
    Label->SetupAttachment(Root);
    Label->SetRelativeLocation(FVector(0.0f, 0.0f, 260.0f));
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetVerticalAlignment(EVRTA_TextCenter);
    Label->SetWorldSize(40.0f);
    Label->SetTextRenderColor(FColor(255, 210, 40));
    Label->SetText(NSLOCTEXT("GrandCityQuest", "GiverDefaultLabel", "QUEST"));
}

void AGrandCityQuestGiver::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (Label)
    {
        const FText DefaultLabel = bIsMinigame
            ? NSLOCTEXT("GrandCityQuest", "GiverMinigameLabel", "MINIGAME")
            : NSLOCTEXT("GrandCityQuest", "GiverDefaultLabel", "QUEST");
        Label->SetText(Quest.Title.IsEmpty() ? DefaultLabel : Quest.Title);
    }
}

void AGrandCityQuestGiver::BeginPlay()
{
    Super::BeginPlay();

    // Hidden until the local player's quest component says this quest is on offer.
    SetLocallyAvailable(false);
    UGrandCityQuestComponent::RefreshLocalQuestVisuals(this);
}

FName AGrandCityQuestGiver::GetQuestId() const
{
    return Quest.QuestId.IsNone() ? GetFName() : Quest.QuestId;
}

bool AGrandCityQuestGiver::IsLocationInTriggerArea(const FVector& WorldLocation, float Tolerance) const
{
    if (!TriggerArea)
    {
        return false;
    }

    const FTransform& BoxTransform = TriggerArea->GetComponentTransform();
    const FVector LocalPoint = BoxTransform.InverseTransformPosition(WorldLocation);
    const FVector Extent = TriggerArea->GetUnscaledBoxExtent();
    const FVector Scale = BoxTransform.GetScale3D().GetAbs();
    // Compare in world units so the tolerance does not shrink or grow with the actor scale.
    return FMath::Abs(LocalPoint.X) * Scale.X <= Extent.X * Scale.X + Tolerance
        && FMath::Abs(LocalPoint.Y) * Scale.Y <= Extent.Y * Scale.Y + Tolerance
        && FMath::Abs(LocalPoint.Z) * Scale.Z <= Extent.Z * Scale.Z + Tolerance;
}

bool AGrandCityQuestGiver::ArePrerequisitesMet(const TArray<FName>& CompletedQuestIds) const
{
    for (const AGrandCityQuestGiver* Required : RequiredQuests)
    {
        if (Required && Required != this && !CompletedQuestIds.Contains(Required->GetQuestId()))
        {
            return false;
        }
    }
    return true;
}

bool AGrandCityQuestGiver::CanBeAcceptedBy(const TArray<FName>& CompletedQuestIds) const
{
    // Minigames stay on offer after every round; quests are done once.
    if (!bIsMinigame && CompletedQuestIds.Contains(GetQuestId()))
    {
        return false;
    }
    return ArePrerequisitesMet(CompletedQuestIds);
}

void AGrandCityQuestGiver::SetLocallyAvailable(bool bAvailable)
{
    // Component visibility is local; actor hidden state would replicate to everyone.
    if (Marker)
    {
        Marker->SetHiddenInGame(!bAvailable);
    }
    if (Label)
    {
        Label->SetHiddenInGame(!bAvailable);
    }
}
