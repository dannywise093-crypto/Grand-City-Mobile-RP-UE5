#include "Quests/GrandCityQuestGiver.h"

#include "Quests/GrandCityQuestComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace GrandCityQuestGiver
{
    const FLinearColor QuestBeaconColor(1.0f, 0.05f, 0.05f);
    const FLinearColor MinigameBeaconColor(1.0f, 0.85f, 0.0f);
    /** Gap between the top of the beacon and the title text. */
    constexpr float LabelAboveBeacon = 60.0f;
}

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
    Marker->SetCanEverAffectNavigation(false);
    Marker->CastShadow = false;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylinderMesh.Succeeded())
    {
        Marker->SetStaticMesh(CylinderMesh.Object);
    }
    // Translucent unlit; colour and opacity come from custom primitive data 0-3 and 4.
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BeaconMaterial(
        TEXT("/Game/Quests/Materials/M_QuestBeacon.M_QuestBeacon"));
    if (BeaconMaterial.Succeeded())
    {
        Marker->SetMaterial(0, BeaconMaterial.Object);
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
    UpdateBeacon();
}

void AGrandCityQuestGiver::UpdateBeacon()
{
    if (!Marker || !TriggerArea)
    {
        return;
    }

    // A tube standing on the bottom of the trigger box, as wide as the box's shorter side.
    const FVector Extent = TriggerArea->GetScaledBoxExtent();
    const FVector Up = TriggerArea->GetUpVector();
    const FVector Bottom = TriggerArea->GetComponentLocation() - Up * Extent.Z;
    const float Diameter = 2.0f * FMath::Min(Extent.X, Extent.Y);
    // The engine cylinder is 100 cm wide and tall, centred on its pivot.
    Marker->SetWorldLocationAndRotation(Bottom + Up * (BeaconHeight * 0.5f), TriggerArea->GetComponentQuat());
    Marker->SetWorldScale3D(FVector(Diameter / 100.0f, Diameter / 100.0f, BeaconHeight / 100.0f));

    const FLinearColor Color = bIsMinigame
        ? GrandCityQuestGiver::MinigameBeaconColor
        : GrandCityQuestGiver::QuestBeaconColor;
    Marker->SetDefaultCustomPrimitiveDataVector4(0, FVector4(Color.R, Color.G, Color.B, 1.0f));
    Marker->SetDefaultCustomPrimitiveDataFloat(4, BeaconOpacity);

    if (Label)
    {
        Label->SetWorldLocation(Bottom + Up * (BeaconHeight + GrandCityQuestGiver::LabelAboveBeacon));
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
