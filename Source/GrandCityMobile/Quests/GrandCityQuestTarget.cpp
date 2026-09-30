#include "Quests/GrandCityQuestTarget.h"

#include "Quests/GrandCityQuestComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Controller.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AGrandCityQuestTarget::AGrandCityQuestTarget()
{
    PrimaryActorTick.bCanEverTick = false;
    // Only the shared destroyed state replicates; everything else is per player.
    bReplicates = true;
    bAlwaysRelevant = true;
    SetNetUpdateFrequency(2.0f);

    RadiusSphere = CreateDefaultSubobject<USphereComponent>(TEXT("RadiusSphere"));
    RootComponent = RadiusSphere;
    RadiusSphere->SetSphereRadius(Radius);
    RadiusSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RadiusSphere->SetGenerateOverlapEvents(false);
    RadiusSphere->SetHiddenInGame(true);
    RadiusSphere->ShapeColor = FColor(40, 200, 255);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(RadiusSphere);
    Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
    Mesh->SetRelativeScale3D(FVector(0.5f));
    Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 25.0f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        Mesh->SetStaticMesh(CubeMesh.Object);
    }

    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
    Label->SetupAttachment(RadiusSphere);
    Label->SetRelativeLocation(FVector(0.0f, 0.0f, 140.0f));
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetVerticalAlignment(EVRTA_TextCenter);
    Label->SetWorldSize(32.0f);
    Label->SetTextRenderColor(FColor(40, 200, 255));

    Beacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon"));
    Beacon->SetupAttachment(RadiusSphere);
    Beacon->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
    Beacon->SetCanEverAffectNavigation(false);
    Beacon->CastShadow = false;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylinderMesh.Succeeded())
    {
        Beacon->SetStaticMesh(CylinderMesh.Object);
    }
    // Translucent unlit; colour and opacity come from custom primitive data 0-3 and 4.
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BeaconMaterial(
        TEXT("/Game/Quests/Materials/M_QuestBeacon.M_QuestBeacon"));
    if (BeaconMaterial.Succeeded())
    {
        Beacon->SetMaterial(0, BeaconMaterial.Object);
    }
}

void AGrandCityQuestTarget::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (RadiusSphere)
    {
        // Radius is in world cm; undo the actor scale so the sphere matches the gameplay check.
        const float Scale = FMath::Max(GetActorScale3D().GetAbsMax(), KINDA_SMALL_NUMBER);
        RadiusSphere->SetSphereRadius(Radius / Scale);
    }
    if (Label)
    {
        Label->SetText(GetDisplayName());
    }
    if (Beacon)
    {
        // Engine cylinder: 100 cm wide and tall, centred. Stand it on the target, as wide as its reach.
        Beacon->SetWorldLocationAndRotation(
            GetActorLocation() + GetActorUpVector() * (BeaconHeight * 0.5f), GetActorQuat());
        Beacon->SetWorldScale3D(FVector(Radius * 2.0f / 100.0f, Radius * 2.0f / 100.0f, BeaconHeight / 100.0f));
        Beacon->SetDefaultCustomPrimitiveDataVector4(0, FVector4(1.0f, 0.45f, 0.0f, 1.0f));
        Beacon->SetDefaultCustomPrimitiveDataFloat(4, BeaconOpacity);
    }
}

void AGrandCityQuestTarget::BeginPlay()
{
    Super::BeginPlay();

    Health = MaxHealth;
    MeshCollision = Mesh ? Mesh->GetCollisionEnabled() : ECollisionEnabled::NoCollision;
    RefreshVisuals();
    UGrandCityQuestComponent::RefreshLocalQuestVisuals(this);
}

void AGrandCityQuestTarget::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(RespawnTimer);
    Super::EndPlay(EndPlayReason);
}

void AGrandCityQuestTarget::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AGrandCityQuestTarget, bDestroyed);
}

FText AGrandCityQuestTarget::GetDisplayName() const
{
    return DisplayName.IsEmpty() ? FText::FromString(GetActorNameOrLabel()) : DisplayName;
}

float AGrandCityQuestTarget::TakeDamage(
    float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (!HasAuthority() || bDestroyed)
    {
        return Applied;
    }

    AController* DamageInstigator = EventInstigator;
    if (!DamageInstigator && DamageCauser)
    {
        DamageInstigator = DamageCauser->GetInstigatorController();
    }

    // Only players with a live Destroy objective on this target can break it.
    if (UGrandCityQuestComponent* QuestComponent = UGrandCityQuestComponent::FindForActor(DamageInstigator))
    {
        QuestComponent->HandleTargetDamaged(this, DamageAmount);
    }
    return Applied;
}

bool AGrandCityQuestTarget::ApplyQuestDamage(float Damage)
{
    if (!HasAuthority() || bDestroyed || Damage <= 0.0f)
    {
        return false;
    }

    Health -= Damage;
    if (Health > 0.0f)
    {
        return false;
    }

    bDestroyed = true;
    OnRep_Destroyed();
    if (RespawnDelaySeconds > 0.0f)
    {
        GetWorldTimerManager().SetTimer(RespawnTimer, this, &AGrandCityQuestTarget::Respawn, RespawnDelaySeconds, false);
    }
    return true;
}

void AGrandCityQuestTarget::Respawn()
{
    if (!HasAuthority())
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(RespawnTimer);
    Health = MaxHealth;
    if (bDestroyed)
    {
        bDestroyed = false;
        OnRep_Destroyed();
    }
}

void AGrandCityQuestTarget::OnRep_Destroyed()
{
    if (Mesh)
    {
        Mesh->SetCollisionEnabled(bDestroyed ? ECollisionEnabled::NoCollision : MeshCollision);
    }
    RefreshVisuals();
}

void AGrandCityQuestTarget::SetLocalQuestState(bool bActive)
{
    bLocallyActive = bActive;
    RefreshVisuals();
}

void AGrandCityQuestTarget::RefreshVisuals()
{
    // Component visibility stays local, so each player only sees what their quest needs.
    const bool bShowMesh = !bDestroyed && (bAlwaysVisible || bLocallyActive);
    if (Mesh)
    {
        Mesh->SetHiddenInGame(!bShowMesh);
    }
    if (Label)
    {
        Label->SetHiddenInGame(bDestroyed || !bLocallyActive);
    }
    if (Beacon)
    {
        Beacon->SetHiddenInGame(bDestroyed || !bLocallyActive);
    }
}
