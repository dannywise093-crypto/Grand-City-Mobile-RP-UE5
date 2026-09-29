#include "Quests/GrandCityQuestCargo.h"

#include "Vehicles/GrandCityVehicle.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace GrandCityQuestCargo
{
    /** Edge length of the box in world cm (the engine cube is 100 cm). */
    constexpr float Size = 60.0f;
    constexpr float LoadDuration = 0.45f;
    /** Peak of the arc onto the roof, so the box does not slide through the car body. */
    constexpr float LoadHopHeight = 150.0f;
    const FLinearColor Color(0.45f, 0.28f, 0.12f);
}

AGrandCityQuestCargo::AGrandCityQuestCargo()
{
    // Only ticks while hopping onto a vehicle.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = false;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
    Mesh->SetCanEverAffectNavigation(false);
    Mesh->SetRelativeScale3D(FVector(GrandCityQuestCargo::Size / 100.0f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        Mesh->SetStaticMesh(CubeMesh.Object);
    }
}

void AGrandCityQuestCargo::BeginPlay()
{
    Super::BeginPlay();

    // Cardboard tint on the engine's basic shape material.
    if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
    {
        Material->SetVectorParameterValue(TEXT("Color"), GrandCityQuestCargo::Color);
    }
}

void AGrandCityQuestCargo::PlaceAt(const FVector& GroundLocation, const FRotator& Rotation)
{
    SetActorTickEnabled(false);
    if (GetAttachParentActor())
    {
        DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    }
    SetActorLocationAndRotation(GroundLocation + FVector(0.0f, 0.0f, GrandCityQuestCargo::Size * 0.5f), Rotation);
}

void AGrandCityQuestCargo::LoadOnto(AGrandCityVehicle* Vehicle, bool bAnimate)
{
    USceneComponent* Parent = Vehicle ? Vehicle->GetRootComponent() : nullptr;
    if (!Parent || GetAttachParentActor() == Vehicle)
    {
        return;
    }

    // Keep the world transform so the hop starts where the box stood.
    AttachToComponent(Parent, FAttachmentTransformRules::KeepWorldTransform);

    const FVector RoofTop = Parent->GetComponentLocation()
        + Parent->GetUpVector() * (Vehicle->GetRoofHeight() + GrandCityQuestCargo::Size * 0.5f);
    LoadTargetLocation = Parent->GetComponentTransform().InverseTransformPosition(RoofTop);
    LoadStartLocation = RootComponent->GetRelativeLocation();
    LoadStartRotation = RootComponent->GetRelativeRotation().Quaternion();
    LoadElapsed = bAnimate ? 0.0f : GrandCityQuestCargo::LoadDuration;

    ApplyLoadMotion();
    SetActorTickEnabled(bAnimate);
}

void AGrandCityQuestCargo::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    LoadElapsed += DeltaSeconds;
    ApplyLoadMotion();
}

void AGrandCityQuestCargo::ApplyLoadMotion()
{
    const float Alpha = FMath::Clamp(LoadElapsed / GrandCityQuestCargo::LoadDuration, 0.0f, 1.0f);
    const float Eased = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);

    FVector Location = FMath::Lerp(LoadStartLocation, LoadTargetLocation, Eased);
    Location.Z += GrandCityQuestCargo::LoadHopHeight * FMath::Sin(PI * Alpha);
    SetActorRelativeLocation(Location);
    SetActorRelativeRotation(FQuat::Slerp(LoadStartRotation, FQuat::Identity, Eased));

    if (Alpha >= 1.0f)
    {
        SetActorTickEnabled(false);
    }
}
