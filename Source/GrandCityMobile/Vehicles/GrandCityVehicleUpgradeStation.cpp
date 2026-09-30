#include "Vehicles/GrandCityVehicleUpgradeStation.h"

#include "Vehicles/GrandCityVehicle.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace GrandCityVehicleUpgradeStation
{
    /** Gap between the top of the beacon and the title text. */
    constexpr float LabelAboveBeacon = 60.0f;
}

AGrandCityVehicleUpgradeStation::AGrandCityVehicleUpgradeStation()
{
    PrimaryActorTick.bCanEverTick = false;
    // Placed in the level on every machine; the upgrade itself replicates on the vehicle.
    bReplicates = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    // Sized for a whole car to park in.
    TriggerArea = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerArea"));
    TriggerArea->SetupAttachment(Root);
    TriggerArea->SetBoxExtent(FVector(350.0f, 350.0f, 150.0f));
    TriggerArea->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
    // Area checks are geometric (IsLocationInTriggerArea), so the box never collides.
    TriggerArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TriggerArea->SetGenerateOverlapEvents(false);
    TriggerArea->ShapeColor = FColor(40, 255, 70);

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
    // Same translucent unlit material as the quest beacons; colour and opacity come from
    // custom primitive data 0-3 and 4.
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BeaconMaterial(
        TEXT("/Game/Quests/Materials/M_QuestBeacon.M_QuestBeacon"));
    if (BeaconMaterial.Succeeded())
    {
        Marker->SetMaterial(0, BeaconMaterial.Object);
    }

    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
    Label->SetupAttachment(Root);
    Label->SetRelativeLocation(FVector(0.0f, 0.0f, 360.0f));
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetVerticalAlignment(EVRTA_TextCenter);
    Label->SetWorldSize(40.0f);
    Label->SetTextRenderColor(FColor(60, 255, 90));
    Label->SetText(StationName);

    // Linear colours, tuned against the pickup's body texture (its shading keeps them close to
    // how they look here).
    const TPair<FText, FLinearColor> DefaultPalette[] = {
        {NSLOCTEXT("GrandCityVehicleUpgrade", "PaintWhite", "White"), FLinearColor(0.8f, 0.8f, 0.8f)},
        {NSLOCTEXT("GrandCityVehicleUpgrade", "PaintBlack", "Black"), FLinearColor(0.02f, 0.02f, 0.02f)},
        {NSLOCTEXT("GrandCityVehicleUpgrade", "PaintRed", "Red"), FLinearColor(0.6f, 0.02f, 0.02f)},
        {NSLOCTEXT("GrandCityVehicleUpgrade", "PaintYellow", "Yellow"), FLinearColor(0.8f, 0.6f, 0.02f)},
        {NSLOCTEXT("GrandCityVehicleUpgrade", "PaintGreen", "Green"), FLinearColor(0.05f, 0.45f, 0.08f)},
        {NSLOCTEXT("GrandCityVehicleUpgrade", "PaintBlue", "Blue"), FLinearColor(0.02f, 0.12f, 0.6f)},
        {NSLOCTEXT("GrandCityVehicleUpgrade", "PaintOrange", "Orange"), FLinearColor(0.8f, 0.25f, 0.01f)},
        {NSLOCTEXT("GrandCityVehicleUpgrade", "PaintPurple", "Purple"), FLinearColor(0.3f, 0.04f, 0.55f)},
        {NSLOCTEXT("GrandCityVehicleUpgrade", "PaintGray", "Gray"), FLinearColor(0.25f, 0.25f, 0.25f)},
    };
    for (const TPair<FText, FLinearColor>& Entry : DefaultPalette)
    {
        FGrandCityPaintOption& Option = PaintPalette.AddDefaulted_GetRef();
        Option.Name = Entry.Key;
        Option.Color = Entry.Value;
    }
}

void AGrandCityVehicleUpgradeStation::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (Label)
    {
        Label->SetText(StationName);
        Label->SetTextRenderColor(BeaconColor.ToFColor(true));
    }
    UpdateBeacon();
}

void AGrandCityVehicleUpgradeStation::UpdateBeacon()
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
    Marker->SetDefaultCustomPrimitiveDataVector4(0, FVector4(BeaconColor.R, BeaconColor.G, BeaconColor.B, 1.0f));
    Marker->SetDefaultCustomPrimitiveDataFloat(4, BeaconOpacity);

    if (Label)
    {
        Label->SetWorldLocation(Bottom + Up * (BeaconHeight + GrandCityVehicleUpgradeStation::LabelAboveBeacon));
    }
}

bool AGrandCityVehicleUpgradeStation::IsLocationInTriggerArea(const FVector& WorldLocation, float Tolerance) const
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

bool AGrandCityVehicleUpgradeStation::CanUpgrade(const AGrandCityVehicle* Vehicle) const
{
    return Vehicle && !Vehicle->HasNitroBoost();
}

bool AGrandCityVehicleUpgradeStation::ApplyUpgrade(AGrandCityVehicle* Vehicle) const
{
    if (!Vehicle || !Vehicle->HasAuthority() || !CanUpgrade(Vehicle))
    {
        return false;
    }

    Vehicle->InstallNitroBoost();
    return true;
}

bool AGrandCityVehicleUpgradeStation::ApplyPaint(AGrandCityVehicle* Vehicle, int32 PaintIndex) const
{
    if (!Vehicle || !Vehicle->HasAuthority() || !PaintPalette.IsValidIndex(PaintIndex))
    {
        return false;
    }

    Vehicle->SetPaintColor(PaintPalette[PaintIndex].Color);
    return true;
}

int32 AGrandCityVehicleUpgradeStation::FindPaintIndex(const AGrandCityVehicle* Vehicle) const
{
    if (!Vehicle || !Vehicle->HasCustomPaint())
    {
        return INDEX_NONE;
    }

    return PaintPalette.IndexOfByPredicate([Vehicle](const FGrandCityPaintOption& Option)
    {
        return Option.Color.Equals(Vehicle->GetPaintColor(), 0.001f);
    });
}
