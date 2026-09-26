#include "GrandCityVehicle.h"

#include "GrandCityMobileCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr float GroundSnapDistance = 45.0f;
constexpr float GroundTraceExtraDepth = 150.0f;
constexpr float GroundAlignSpeed = 10.0f;
constexpr float ServerUpdateInterval = 1.0f / 30.0f;
constexpr float ProxyInterpSpeed = 12.0f;
constexpr float ProxySnapDistance = 1000.0f;
constexpr int32 MaxSlideIterations = 3;
}

AGrandCityVehicle::AGrandCityVehicle()
{
    PrimaryActorTick.bCanEverTick = true;

    bReplicates = true;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(30.0f);

    // The vehicle steers itself; controller rotation only drives the camera.
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;
    AutoPossessAI = EAutoPossessAI::Disabled;

    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    SetRootComponent(CollisionBox);
    CollisionBox->SetCollisionProfileName(UCollisionProfile::Vehicle_ProfileName);
    CollisionBox->SetCanEverAffectNavigation(false);
    CollisionBox->CanCharacterStepUpOn = ECB_No;
    CollisionBox->SetBoxExtent(FVector(215.0f, 96.0f, 60.0f));

    VehicleBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VehicleBody"));
    VehicleBody->SetupAttachment(CollisionBox);
    VehicleBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VehicleBody->SetCanEverAffectNavigation(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> VehicleMesh(
        TEXT("/Game/VehicleVarietyPack/Meshes/SM_Pickup.SM_Pickup"));
    if (VehicleMesh.Succeeded())
    {
        VehicleBody->SetStaticMesh(VehicleMesh.Object);
    }

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(CollisionBox);
    CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
    CameraBoom->SetRelativeRotation(FRotator(-12.0f, 0.0f, 0.0f));
    CameraBoom->TargetArmLength = 750.0f;
    CameraBoom->bUsePawnControlRotation = false;
    CameraBoom->bInheritPitch = false;
    CameraBoom->bInheritRoll = false;
    CameraBoom->bInheritYaw = true;
    CameraBoom->bDoCollisionTest = true;
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 10.0f;
    CameraBoom->bEnableCameraRotationLag = true;
    CameraBoom->CameraRotationLagSpeed = 5.0f;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    FitCollisionToMesh();
}

void AGrandCityVehicle::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    FitCollisionToMesh();
}

void AGrandCityVehicle::BeginPlay()
{
    Super::BeginPlay();
    FitCollisionToMesh();
}

void AGrandCityVehicle::FitCollisionToMesh()
{
    const UStaticMesh* Mesh = VehicleBody ? VehicleBody->GetStaticMesh() : nullptr;
    if (!Mesh || !CollisionBox)
    {
        return;
    }

    // The box starts GroundClearance above the wheels so low curbs and road seams
    // pass underneath; ground traces keep the wheels on the surface instead.
    // GroundClearance is in world centimeters, so convert it into the actor's scaled space.
    const float ScaleZ = FMath::Max(GetActorScale3D().Z, KINDA_SMALL_NUMBER);
    const FRotator MeshRotation(0.0f, MeshYawOffset, 0.0f);
    const FBox Bounds = Mesh->GetBoundingBox().TransformBy(FTransform(MeshRotation));
    const float BottomZ = Bounds.Min.Z + GroundClearance / ScaleZ;
    const float TopZ = FMath::Max(Bounds.Max.Z, BottomZ + 20.0f);
    const FVector Extent(Bounds.GetExtent().X, Bounds.GetExtent().Y, (TopZ - BottomZ) * 0.5f);
    const FVector BoxCenter(Bounds.GetCenter().X, Bounds.GetCenter().Y, (TopZ + BottomZ) * 0.5f);

    CollisionBox->SetBoxExtent(Extent, false);
    VehicleBody->SetRelativeLocationAndRotation(-BoxCenter, MeshRotation);
}

void AGrandCityVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAxis(TEXT("VehicleThrottle"), this, &AGrandCityVehicle::KeyboardThrottle);
    PlayerInputComponent->BindAxis(TEXT("VehicleSteer"), this, &AGrandCityVehicle::KeyboardSteer);
    PlayerInputComponent->BindAction(TEXT("VehicleBrake"), IE_Pressed, this, &AGrandCityVehicle::KeyboardBrakePressed);
    PlayerInputComponent->BindAction(TEXT("VehicleBrake"), IE_Released, this, &AGrandCityVehicle::KeyboardBrakeReleased);
}

void AGrandCityVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AGrandCityVehicle, Driver);
}

void AGrandCityVehicle::KeyboardThrottle(float Value)
{
    KeyboardThrottleInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void AGrandCityVehicle::KeyboardSteer(float Value)
{
    KeyboardSteeringInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void AGrandCityVehicle::KeyboardBrakePressed()
{
    bKeyboardBrakeHeld = true;
}

void AGrandCityVehicle::KeyboardBrakeReleased()
{
    bKeyboardBrakeHeld = false;
}

void AGrandCityVehicle::ClearDriverInput()
{
    KeyboardThrottleInput = 0.0f;
    KeyboardSteeringInput = 0.0f;
    TouchThrottleInput = 0.0f;
    TouchSteeringInput = 0.0f;
    bKeyboardBrakeHeld = false;
    bTouchBrakeHeld = false;
}

void AGrandCityVehicle::SetDriver(AGrandCityMobileCharacter* NewDriver)
{
    if (!HasAuthority())
    {
        return;
    }

    Driver = NewDriver;
    ClearDriverInput();
    ForceNetUpdate();
}

void AGrandCityVehicle::UnPossessed()
{
    Super::UnPossessed();
    ClearDriverInput();
}

void AGrandCityVehicle::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (DeltaSeconds <= 0.0f)
    {
        return;
    }

    if (IsLocallyControlled())
    {
        const float Throttle = FMath::Clamp(KeyboardThrottleInput + TouchThrottleInput, -1.0f, 1.0f);
        const float Steering = FMath::Clamp(KeyboardSteeringInput + TouchSteeringInput, -1.0f, 1.0f);
        SimulateMovement(DeltaSeconds, Throttle, Steering, bKeyboardBrakeHeld || bTouchBrakeHeld);

        if (!HasAuthority())
        {
            TimeSinceServerUpdate += DeltaSeconds;
            if (TimeSinceServerUpdate >= ServerUpdateInterval)
            {
                TimeSinceServerUpdate = 0.0f;
                ServerUpdateMovement(GetActorLocation(), GetActorRotation(), ForwardSpeed);
            }
        }
    }
    else if (HasAuthority())
    {
        // A remote driver streams its transform through ServerUpdateMovement.
        // Without a driver the server rolls the car to a stop and settles it.
        const bool bSettled = bIsGrounded
            && FMath::IsNearlyZero(ForwardSpeed)
            && FMath::IsNearlyZero(VerticalSpeed);
        if (!Controller && !bSettled)
        {
            SimulateMovement(DeltaSeconds, 0.0f, 0.0f, true);
        }
    }
    else
    {
        TickSimulatedProxy(DeltaSeconds);
    }
}

void AGrandCityVehicle::UpdateSpeed(float DeltaSeconds, float Throttle, bool bBrake)
{
    if (bBrake)
    {
        ForwardSpeed = FMath::FInterpConstantTo(ForwardSpeed, 0.0f, DeltaSeconds, BrakeDeceleration);
    }
    else if (Throttle > KINDA_SMALL_NUMBER)
    {
        if (ForwardSpeed < -1.0f)
        {
            // Pressing forward while rolling backward brakes first.
            ForwardSpeed = FMath::FInterpConstantTo(ForwardSpeed, 0.0f, DeltaSeconds, BrakeDeceleration);
        }
        else if (ForwardSpeed < MaxForwardSpeed)
        {
            // Acceleration tapers off toward top speed.
            const float SpeedAlpha = FMath::Clamp(ForwardSpeed / MaxForwardSpeed, 0.0f, 1.0f);
            const float CurrentAcceleration = Acceleration * (1.0f - 0.6f * SpeedAlpha);
            ForwardSpeed = FMath::Min(ForwardSpeed + CurrentAcceleration * Throttle * DeltaSeconds, MaxForwardSpeed);
        }
    }
    else if (Throttle < -KINDA_SMALL_NUMBER)
    {
        if (ForwardSpeed > 1.0f)
        {
            // Reverse while moving forward acts as a brake until the car stops.
            ForwardSpeed = FMath::FInterpConstantTo(ForwardSpeed, 0.0f, DeltaSeconds, BrakeDeceleration);
        }
        else
        {
            ForwardSpeed = FMath::Max(ForwardSpeed + ReverseAcceleration * Throttle * DeltaSeconds, -MaxReverseSpeed);
        }
    }
    else
    {
        ForwardSpeed = FMath::FInterpConstantTo(ForwardSpeed, 0.0f, DeltaSeconds, CoastDeceleration);
    }
}

void AGrandCityVehicle::SimulateMovement(float DeltaSeconds, float Throttle, float Steering, bool bBrake)
{
    UpdateSpeed(DeltaSeconds, Throttle, bBrake);

    // Steering authority grows with speed (a parked car can't spin in place) and
    // eases off near top speed so fast turns stay controllable.
    SmoothedSteering = FMath::FInterpConstantTo(SmoothedSteering, Steering, DeltaSeconds, SteeringResponse);
    const float AbsSpeed = FMath::Abs(ForwardSpeed);
    const float LowSpeedScale = FMath::Clamp(AbsSpeed / FMath::Max(FullSteeringSpeed, 1.0f), 0.0f, 1.0f);
    const float HighSpeedAlpha = FMath::Clamp(
        (AbsSpeed - FullSteeringSpeed) / FMath::Max(MaxForwardSpeed - FullSteeringSpeed, 1.0f), 0.0f, 1.0f);
    const float SteeringScale = LowSpeedScale * FMath::Lerp(1.0f, HighSpeedSteeringFactor, HighSpeedAlpha);
    const float YawDelta = SmoothedSteering * MaxSteeringRate * SteeringScale * FMath::Sign(ForwardSpeed) * DeltaSeconds;

    const FQuat CurrentRotation = GetActorQuat();
    const FQuat YawedRotation = FQuat(FVector::UpVector, FMath::DegreesToRadians(YawDelta)) * CurrentRotation;
    const FVector Forward = YawedRotation.GetForwardVector();
    const FVector FlatForward = FVector(Forward.X, Forward.Y, 0.0f).GetSafeNormal();

    FVector Delta = Forward * ForwardSpeed * DeltaSeconds;
    const FVector PredictedLocation = GetActorLocation() + Delta;

    // Sample the ground under all four corners at the predicted position.
    const FVector Extent = CollisionBox->GetScaledBoxExtent();
    const FVector FlatRight = FVector::CrossProduct(FVector::UpVector, FlatForward);
    const FVector CornerOffsets[4] = {
        FlatForward * Extent.X * 0.8f + FlatRight * Extent.Y * 0.8f,
        FlatForward * Extent.X * 0.8f - FlatRight * Extent.Y * 0.8f,
        -FlatForward * Extent.X * 0.8f + FlatRight * Extent.Y * 0.8f,
        -FlatForward * Extent.X * 0.8f - FlatRight * Extent.Y * 0.8f,
    };

    FVector Impacts[4];
    bool bHits[4];
    int32 HitCount = 0;
    float GroundZSum = 0.0f;
    FVector NormalSum = FVector::ZeroVector;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        FVector Normal;
        bHits[Index] = TraceGround(PredictedLocation + CornerOffsets[Index], Impacts[Index], Normal);
        if (bHits[Index])
        {
            ++HitCount;
            GroundZSum += Impacts[Index].Z;
            NormalSum += Normal;
        }
    }

    const float RideHeight = Extent.Z + GroundClearance + RideHeightOffset;
    const float CurrentZ = GetActorLocation().Z;
    FQuat TargetRotation = YawedRotation;
    bIsGrounded = false;

    if (HitCount > 0)
    {
        const float GroundZ = GroundZSum / HitCount;
        const float TargetZ = GroundZ + RideHeight;

        if (VerticalSpeed <= 0.0f && CurrentZ + VerticalSpeed * DeltaSeconds <= TargetZ + GroundSnapDistance)
        {
            bIsGrounded = true;
            VerticalSpeed = 0.0f;
            Delta.Z = TargetZ - CurrentZ;

            FVector GroundNormal = NormalSum.GetSafeNormal();
            if (HitCount == 4)
            {
                // The diagonals of the four contact points give a steadier plane than hit normals.
                const FVector DiagonalA = Impacts[0] - Impacts[3];
                const FVector DiagonalB = Impacts[1] - Impacts[2];
                const FVector PlaneNormal = FVector::CrossProduct(DiagonalB, DiagonalA).GetSafeNormal();
                if (!PlaneNormal.IsNearlyZero())
                {
                    GroundNormal = PlaneNormal.Z >= 0.0f ? PlaneNormal : -PlaneNormal;
                }
            }

            if (GroundNormal.Z > 0.5f)
            {
                const FQuat AlignedRotation = FRotationMatrix::MakeFromXZ(FlatForward, GroundNormal).ToQuat();
                TargetRotation = FQuat::Slerp(
                    YawedRotation, AlignedRotation, FMath::Clamp(DeltaSeconds * GroundAlignSpeed, 0.0f, 1.0f));
            }
        }
    }

    if (!bIsGrounded)
    {
        VerticalSpeed -= Gravity * DeltaSeconds;
        Delta.Z = VerticalSpeed * DeltaSeconds;

        // Level out slowly in the air.
        const FQuat LevelRotation = FRotationMatrix::MakeFromXZ(FlatForward, FVector::UpVector).ToQuat();
        TargetRotation = FQuat::Slerp(YawedRotation, LevelRotation, FMath::Clamp(DeltaSeconds * 2.0f, 0.0f, 1.0f));
    }

    MoveWithCollision(Delta, TargetRotation);
}

bool AGrandCityVehicle::TraceGround(const FVector& WorldPoint, FVector& OutImpact, FVector& OutNormal) const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    const FVector Extent = CollisionBox->GetScaledBoxExtent();
    const FVector Start(WorldPoint.X, WorldPoint.Y, WorldPoint.Z + Extent.Z);
    const FVector End(WorldPoint.X, WorldPoint.Y, WorldPoint.Z - Extent.Z - GroundClearance - FMath::Max(RideHeightOffset, 0.0f) - GroundTraceExtraDepth);

    FCollisionQueryParams Params(SCENE_QUERY_STAT(GrandCityVehicleGround), false, this);
    if (Driver)
    {
        Params.AddIgnoredActor(Driver);
    }

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

    // Object-type queries ignore collision responses, so they also report query-only
    // volumes (PCG, post process, audio...) and hits at the trace start when it begins
    // inside one of them. Only accept surfaces the vehicle body would actually block on.
    TArray<FHitResult> Hits;
    World->LineTraceMultiByObjectType(Hits, Start, End, ObjectParams, Params);

    const ECollisionChannel VehicleChannel = CollisionBox->GetCollisionObjectType();
    for (const FHitResult& Hit : Hits)
    {
        const UPrimitiveComponent* HitComponent = Hit.GetComponent();
        if (Hit.bStartPenetrating
            || !HitComponent
            || HitComponent->GetCollisionResponseToChannel(VehicleChannel) != ECR_Block)
        {
            continue;
        }

        OutImpact = Hit.ImpactPoint;
        OutNormal = Hit.ImpactNormal;
        return true;
    }

    return false;
}

void AGrandCityVehicle::MoveWithCollision(const FVector& Delta, const FQuat& NewRotation)
{
    FVector RemainingDelta = Delta;
    FQuat Rotation = NewRotation;

    for (int32 Iteration = 0; Iteration < MaxSlideIterations; ++Iteration)
    {
        FHitResult Hit;
        SetActorLocationAndRotation(GetActorLocation() + RemainingDelta, Rotation, true, &Hit);
        if (!Hit.bBlockingHit)
        {
            return;
        }

        if (Hit.bStartPenetrating)
        {
            // Already overlapping something (usually after turning into a wall): push out.
            SetActorLocationAndRotation(
                GetActorLocation() + Hit.Normal * (Hit.PenetrationDepth + 1.0f), Rotation, false);
            RemainingDelta = FVector::VectorPlaneProject(RemainingDelta, Hit.Normal);
            continue;
        }

        // Head-on impacts kill speed, glancing ones keep most of it. The normal is
        // left unnormalized so floor-like hits barely affect speed.
        const FVector FlatNormal(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.0f);
        const FVector MoveDirection = GetActorForwardVector() * FMath::Sign(ForwardSpeed);
        const float ImpactFactor = FMath::Clamp(FVector::DotProduct(MoveDirection, -FlatNormal), 0.0f, 1.0f);
        ForwardSpeed *= 1.0f - ImpactFactor;

        RemainingDelta = FVector::VectorPlaneProject(RemainingDelta * (1.0f - Hit.Time), Hit.Normal);
        Rotation = GetActorQuat();
        if (RemainingDelta.IsNearlyZero(0.1f))
        {
            return;
        }
    }
}

void AGrandCityVehicle::ServerUpdateMovement_Implementation(
    FVector_NetQuantize10 NewLocation,
    FRotator NewRotation,
    float NewForwardSpeed)
{
    // Reject obviously impossible jumps (packets from a stale session, cheats).
    const float MaxStep = FMath::Max(MaxForwardSpeed, MaxReverseSpeed) + 1000.0f;
    if (FVector::DistSquared(GetActorLocation(), NewLocation) > FMath::Square(MaxStep))
    {
        return;
    }

    ForwardSpeed = FMath::Clamp(NewForwardSpeed, -MaxReverseSpeed, MaxForwardSpeed);
    bIsGrounded = false;
    SetActorLocationAndRotation(NewLocation, NewRotation, false);
}

void AGrandCityVehicle::PostNetReceiveLocationAndRotation()
{
    if (IsLocallyControlled())
    {
        // The driving client owns its transform.
        return;
    }

    if (GetLocalRole() == ROLE_SimulatedProxy)
    {
        const FRepMovement& RepMovement = GetReplicatedMovement();
        ProxyTargetLocation = FRepMovement::RebaseOntoLocalOrigin(RepMovement.Location, this);
        ProxyTargetRotation = RepMovement.Rotation;

        const bool bFarAway = FVector::DistSquared(GetActorLocation(), ProxyTargetLocation)
            > FMath::Square(ProxySnapDistance);
        if (!bHasProxyTarget || bFarAway)
        {
            Super::PostNetReceiveLocationAndRotation();
        }
        bHasProxyTarget = true;
        return;
    }

    Super::PostNetReceiveLocationAndRotation();
}

void AGrandCityVehicle::TickSimulatedProxy(float DeltaSeconds)
{
    if (!bHasProxyTarget)
    {
        return;
    }

    const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), ProxyTargetLocation, DeltaSeconds, ProxyInterpSpeed);
    const FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), ProxyTargetRotation, DeltaSeconds, ProxyInterpSpeed);
    SetActorLocationAndRotation(NewLocation, NewRotation, false);
}

float AGrandCityVehicle::GetDistanceToVehicle(const FVector& WorldPoint) const
{
    const FVector LocalPoint = GetActorTransform().InverseTransformPositionNoScale(WorldPoint);
    const FVector Extent = CollisionBox->GetScaledBoxExtent();
    const FVector Closest(
        FMath::Clamp(LocalPoint.X, -Extent.X, Extent.X),
        FMath::Clamp(LocalPoint.Y, -Extent.Y, Extent.Y),
        FMath::Clamp(LocalPoint.Z, -Extent.Z, Extent.Z));
    return FVector::Dist(LocalPoint, Closest);
}

bool AGrandCityVehicle::FindExitTransform(
    const AGrandCityMobileCharacter* Character,
    FVector& OutLocation,
    FRotator& OutRotation) const
{
    const UWorld* World = GetWorld();
    const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
    if (!World || !Capsule)
    {
        return false;
    }

    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const FVector Extent = CollisionBox->GetScaledBoxExtent();
    const FVector Forward = FVector(GetActorForwardVector().X, GetActorForwardVector().Y, 0.0f).GetSafeNormal();
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
    const FVector Center = GetActorLocation();
    const float SideGap = Radius + 30.0f;

    // Driver door first, then the other side, behind, and in front.
    const FVector Candidates[] = {
        Center - Right * (Extent.Y + SideGap) + Forward * (Extent.X * 0.15f),
        Center + Right * (Extent.Y + SideGap) + Forward * (Extent.X * 0.15f),
        Center - Forward * (Extent.X + SideGap),
        Center + Forward * (Extent.X + SideGap),
    };

    FCollisionQueryParams Params(SCENE_QUERY_STAT(GrandCityVehicleExit), false, this);
    Params.AddIgnoredActor(Character);
    const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

    OutRotation = FRotator(0.0f, GetActorRotation().Yaw, 0.0f);
    for (const FVector& Candidate : Candidates)
    {
        // Find the floor under the candidate spot.
        FHitResult FloorHit;
        const FVector TraceStart = Candidate + FVector(0.0f, 0.0f, Extent.Z + HalfHeight);
        const FVector TraceEnd = Candidate - FVector(0.0f, 0.0f, Extent.Z + GroundClearance + 200.0f);
        if (!World->LineTraceSingleByChannel(FloorHit, TraceStart, TraceEnd, ECC_Pawn, Params))
        {
            continue;
        }

        const FVector StandLocation = FloorHit.ImpactPoint + FVector(0.0f, 0.0f, HalfHeight + 5.0f);
        if (World->OverlapBlockingTestByChannel(StandLocation, FQuat::Identity, ECC_Pawn, CapsuleShape, Params))
        {
            continue;
        }

        // Don't let the driver step out through a wall.
        FHitResult WallHit;
        const FVector SightStart(Center.X, Center.Y, StandLocation.Z);
        if (World->LineTraceSingleByChannel(WallHit, SightStart, StandLocation, ECC_Pawn, Params))
        {
            continue;
        }

        OutLocation = StandLocation;
        return true;
    }

    // Last resort: the roof.
    const FVector RoofLocation = Center + FVector(0.0f, 0.0f, Extent.Z + HalfHeight + 10.0f);
    if (!World->OverlapBlockingTestByChannel(RoofLocation, FQuat::Identity, ECC_Pawn, CapsuleShape, Params))
    {
        OutLocation = RoofLocation;
        return true;
    }

    return false;
}
