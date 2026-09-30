#include "GrandCityMobileCharacter.h"
#include "Vehicles/GrandCityVehicle.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/InputComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#if !UE_BUILD_SHIPPING
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#endif

namespace
{
constexpr float CrouchAnimationBlendTime = 0.2f;
constexpr float CrouchMovementPauseDuration = CrouchAnimationBlendTime + 0.05f;
constexpr float CrouchTransitionMoveSpeed = 20.0f;
constexpr int32 CrouchAnimationLoopCount = 100000;
const FName CrouchAnimationSlotName(TEXT("DefaultSlot"));
constexpr float VehicleMontageBlendTime = 0.25f;

/** Component-space location of a bone in an animation at the given time (composes up to the root). */
bool SampleComponentBoneLocation(const UAnimSequence* Animation, FName BoneName, double Time, FVector& OutLocation)
{
    const USkeleton* Skeleton = Animation ? Animation->GetSkeleton() : nullptr;
    if (!Skeleton)
    {
        return false;
    }

    const FReferenceSkeleton& ReferenceSkeleton = Skeleton->GetReferenceSkeleton();
    int32 BoneIndex = ReferenceSkeleton.FindBoneIndex(BoneName);
    if (BoneIndex == INDEX_NONE)
    {
        return false;
    }

    const FAnimExtractContext ExtractContext(Time);
    FTransform ComponentTransform = FTransform::Identity;
    while (BoneIndex != INDEX_NONE)
    {
        FTransform LocalTransform;
        Animation->GetBoneTransform(LocalTransform, FSkeletonPoseBoneIndex(BoneIndex), ExtractContext, false);
        ComponentTransform = ComponentTransform * LocalTransform;
        BoneIndex = ReferenceSkeleton.GetParentIndex(BoneIndex);
    }

    OutLocation = ComponentTransform.GetLocation();
    return true;
}
}

#if !UE_BUILD_SHIPPING
DEFINE_LOG_CATEGORY_STATIC(LogGrandCityCrouchPlaytest, Log, All);
#endif

AGrandCityMobileCharacter::AGrandCityMobileCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    bReplicates = true;
    SetReplicateMovement(true);

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> CharacterMesh(
        TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
    if (CharacterMesh.Succeeded())
    {
        GetMesh()->SetSkeletalMeshAsset(CharacterMesh.Object);
        GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
        GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    }

    static ConstructorHelpers::FClassFinder<UAnimInstance> CharacterAnimation(
        TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
    if (CharacterAnimation.Succeeded())
    {
        StandingAnimInstanceClass = CharacterAnimation.Class;
        GetMesh()->SetAnimInstanceClass(CharacterAnimation.Class);
    }

    static ConstructorHelpers::FObjectFinder<UAnimSequence> CrouchEntry(
        TEXT("/Game/Characters/Mannequins/Anims/CrouchFixed/MM_Unarmed_Crouch_Entry.MM_Unarmed_Crouch_Entry"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> CrouchExit(
        TEXT("/Game/Characters/Mannequins/Anims/CrouchFixed/MM_Unarmed_Crouch_Exit.MM_Unarmed_Crouch_Exit"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> CrouchIdle(
        TEXT("/Game/Characters/Mannequins/Anims/CrouchFixed/MM_Unarmed_Crouch_Idle.MM_Unarmed_Crouch_Idle"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> CrouchWalkForward(
        TEXT("/Game/Characters/Mannequins/Anims/CrouchFixed/MM_Unarmed_Crouch_Walk_Fwd.MM_Unarmed_Crouch_Walk_Fwd"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> CrouchWalkBackward(
        TEXT("/Game/Characters/Mannequins/Anims/CrouchFixed/MM_Unarmed_Crouch_Walk_Bwd.MM_Unarmed_Crouch_Walk_Bwd"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> CrouchWalkLeft(
        TEXT("/Game/Characters/Mannequins/Anims/CrouchFixed/MM_Unarmed_Crouch_Walk_Left.MM_Unarmed_Crouch_Walk_Left"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> CrouchWalkRight(
        TEXT("/Game/Characters/Mannequins/Anims/CrouchFixed/MM_Unarmed_Crouch_Walk_Right.MM_Unarmed_Crouch_Walk_Right"));

    static ConstructorHelpers::FObjectFinder<UAnimSequence> CarEnter(
        TEXT("/Game/Characters/Mannequins/Anims/Vehicle/MM_Car_Enter.MM_Car_Enter"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> CarExit(
        TEXT("/Game/Characters/Mannequins/Anims/Vehicle/MM_Car_Exit.MM_Car_Exit"));
    EnterVehicleAnimation = CarEnter.Object;
    ExitVehicleAnimation = CarExit.Object;

    CrouchEntryAnimation = CrouchEntry.Object;
    CrouchExitAnimation = CrouchExit.Object;
    CrouchIdleAnimation = CrouchIdle.Object;
    CrouchWalkForwardAnimation = CrouchWalkForward.Object;
    CrouchWalkBackwardAnimation = CrouchWalkBackward.Object;
    CrouchWalkLeftAnimation = CrouchWalkLeft.Object;
    CrouchWalkRightAnimation = CrouchWalkRight.Object;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 350.0f;
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
    GetCharacterMovement()->SetCrouchedHalfHeight(60.0f);
    GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
    GetCharacterMovement()->BrakingDecelerationWalking = 1800.0f;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
}

void AGrandCityMobileCharacter::BeginPlay()
{
    Super::BeginPlay();

#if !UE_BUILD_SHIPPING
    bCrouchMovementPlaytestEnabled = FParse::Param(
        FCommandLine::Get(), TEXT("GrandCityCrouchPlaytest"));
#endif

    if (USkeletalMeshComponent* MeshComponent = GetMesh())
    {
        StandingAnimInstanceClass = MeshComponent->GetAnimClass();
        DefaultMeshRelativeLocation = MeshComponent->GetRelativeLocation();
        DefaultMeshRelativeRotation = MeshComponent->GetRelativeRotation();
        BoneTransformsFinalizedHandle = MeshComponent->RegisterOnBoneTransformsFinalizedDelegate(
            FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(
                this, &AGrandCityMobileCharacter::HandleBoneTransformsFinalized));
    }
}

void AGrandCityMobileCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

#if !UE_BUILD_SHIPPING
    TickCrouchMovementPlaytest();
#endif

    if (IsInVehicleTransition())
    {
        TickVehicleTransition(DeltaSeconds);
        return;
    }

    if (bCrouchMovementPaused)
    {
        // A previous transition already owns the pause timer.
        return;
    }

    if ((CrouchAnimationPhase == EGrandCityCrouchAnimationPhase::Entering
            || CrouchAnimationPhase == EGrandCityCrouchAnimationPhase::Exiting)
        && IsMovingForCrouchTransition())
    {
        const float RemainingAnimationTime = FMath::Max(
            GetWorldTimerManager().GetTimerRemaining(CrouchAnimationTimer), 0.0f);
        PauseMovementForCrouchTransition(RemainingAnimationTime + CrouchMovementPauseDuration);
    }

    if (bCrouchMovementPaused)
    {
        // Entering or exiting may have started a new pause above.
        return;
    }

    if (CrouchAnimationPhase == EGrandCityCrouchAnimationPhase::Crouched)
    {
        if (bIsCrouched)
        {
            UpdateCrouchLocomotion();
        }
        else
        {
            RestoreStandingAnimation();
        }
    }
}

void AGrandCityMobileCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AGrandCityMobileCharacter::MoveForward);
    PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AGrandCityMobileCharacter::MoveRight);
    PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AGrandCityMobileCharacter::Turn);
    PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &AGrandCityMobileCharacter::LookUp);
    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ACharacter::Jump);
    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
    PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Pressed, this, &AGrandCityMobileCharacter::StartSprint);
    PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Released, this, &AGrandCityMobileCharacter::StopSprint);
    PlayerInputComponent->BindAction(TEXT("Crouch"), IE_Pressed, this, &AGrandCityMobileCharacter::ToggleCrouch);
}

void AGrandCityMobileCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AGrandCityMobileCharacter, bIsSprinting);
    DOREPLIFETIME(AGrandCityMobileCharacter, OccupiedVehicle);
}

void AGrandCityMobileCharacter::EnterVehicle(AGrandCityVehicle* Vehicle)
{
    if (!HasAuthority() || !Vehicle || OccupiedVehicle)
    {
        return;
    }

    if (bIsCrouched || WantsToCrouch())
    {
        UnCrouch();
    }
    bIsSprinting = false;
    ForwardInputValue = 0.0f;
    RightInputValue = 0.0f;

    OccupiedVehicle = Vehicle;
    // Sit in the driver seat and ride along with the car. With the default mesh offset
    // the animation root lands on the seat, where the held seated pose expects it.
    AttachToActor(Vehicle, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    const FTransform Seat = Vehicle->GetDriverSeatTransform(true);
    SetActorLocationAndRotation(
        Seat.GetLocation() - Seat.GetRotation().GetUpVector() * DefaultMeshRelativeLocation.Z,
        Seat.GetRotation());
    ApplyOccupiedVehicleState();
    ForceNetUpdate();
}

void AGrandCityMobileCharacter::ExitVehicle(const FVector& ExitLocation, const FRotator& ExitRotation)
{
    if (!HasAuthority() || !OccupiedVehicle)
    {
        return;
    }

    OccupiedVehicle = nullptr;
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    SetActorLocationAndRotation(ExitLocation, ExitRotation, false, nullptr, ETeleportType::TeleportPhysics);
    ApplyOccupiedVehicleState();
    ForceNetUpdate();
}

void AGrandCityMobileCharacter::OnRep_OccupiedVehicle()
{
    ApplyOccupiedVehicleState();
}

void AGrandCityMobileCharacter::ApplyOccupiedVehicleState()
{
    const bool bInVehicle = OccupiedVehicle != nullptr;
    if (bInVehicle && VehicleTransition == EGrandCityVehicleTransition::Entering)
    {
        // Seated: the seated pose below replaces the enter clip's last frame.
        EndVehicleTransition(false);
    }

    // In the car and during enter/exit clips the character stays frozen and
    // non-colliding (the multicast and this OnRep may arrive in either order on
    // clients, hence the transition check).
    const bool bFrozen = bInVehicle || IsInVehicleTransition();
    SetActorHiddenInGame(false);
    SetActorEnableCollision(!bFrozen);
    ApplyMovementSpeed();

    if (bInVehicle)
    {
        PlaySeatedPose();
    }
    else if (!IsInVehicleTransition())
    {
        // Instant exit (no clip): drop the seated pose straight away.
        StopSeatedPose();
        RestoreDefaultMeshPlacement();
    }

    if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
    {
        if (bFrozen)
        {
            MovementComponent->StopMovementImmediately();
            MovementComponent->DisableMovement();
        }
        else if (MovementComponent->MovementMode == MOVE_None)
        {
            // Walking re-checks the floor and falls if the exit spot is above ground.
            MovementComponent->SetMovementMode(MOVE_Walking);
        }
    }
}

float AGrandCityMobileCharacter::PlayEnterVehicleAnimation(AGrandCityVehicle* Vehicle)
{
    if (!HasAuthority() || !Vehicle || !EnterVehicleAnimation || IsInVehicleTransition()
        || bIsCrouched || WantsToCrouch())
    {
        return 0.0f;
    }

    // Work backwards from the seat: the clip starts beside the driver door, facing the
    // car, and walks the pelvis EnterAnimationSeatOffset into the seat.
    const FTransform Seat = Vehicle->GetDriverSeatTransform();
    const FRotator StartRotation(0.0f, Seat.Rotator().Yaw + EnterAnimationStartYaw, 0.0f);
    const FVector StartFeet = Seat.GetLocation()
        - StartRotation.RotateVector(FVector(EnterAnimationSeatOffset.X, EnterAnimationSeatOffset.Y, 0.0f));

    FVector StartLocation;
    if (!FindVehicleStandSpot(Vehicle, StartFeet, StartLocation))
    {
        return 0.0f;
    }

    MulticastPlayEnterVehicle(StartLocation, StartRotation.Yaw, Seat.GetLocation().Z);
    return GetVehicleAnimationDuration(EnterVehicleAnimation);
}

bool AGrandCityMobileCharacter::FindAnimatedVehicleExit(
    const AGrandCityVehicle* Vehicle,
    FVector& OutLocation,
    FRotator& OutRotation) const
{
    if (!Vehicle || !ExitVehicleAnimation)
    {
        return false;
    }

    // The capsule must stand exactly under the pelvis at the moment the clip starts
    // blending out (auto blend-out begins with VehicleMontageBlendTime of play time left),
    // so the hand-off back to locomotion doesn't have to slide the body. Sample that
    // from the clip; ExitAnimationEndOffset is only the fallback.
    FVector SeatSpaceOffset(ExitAnimationEndOffset.X, ExitAnimationEndOffset.Y, 0.0f);
    const double BlendOutStartTime = FMath::Max(
        0.0, ExitVehicleAnimation->GetPlayLength() - VehicleMontageBlendTime * FMath::Max(VehicleAnimationPlayRate, 0.1f));
    FVector PelvisComponent;
    if (SampleComponentBoneLocation(ExitVehicleAnimation, TEXT("pelvis"), BlendOutStartTime, PelvisComponent))
    {
        // Mesh (component) space to the seat's actor-style space (X forward, Y right).
        SeatSpaceOffset = DefaultMeshRelativeRotation.RotateVector(PelvisComponent);
        SeatSpaceOffset.Z = 0.0f;
    }

    const FTransform Seat = Vehicle->GetDriverSeatTransform();
    const FVector EndFeet = Seat.TransformPositionNoScale(SeatSpaceOffset);
    if (!FindVehicleStandSpot(Vehicle, EndFeet, OutLocation))
    {
        return false;
    }

    OutRotation = FRotator(0.0f, Seat.Rotator().Yaw + ExitAnimationEndYaw, 0.0f);
    return true;
}

float AGrandCityMobileCharacter::PlayExitVehicleAnimation(const AGrandCityVehicle* Vehicle)
{
    if (!HasAuthority() || !Vehicle || !ExitVehicleAnimation)
    {
        return 0.0f;
    }

    const FTransform Seat = Vehicle->GetDriverSeatTransform();
    MulticastPlayExitVehicle(Seat.GetLocation(), Seat.Rotator().Yaw);
    return GetVehicleAnimationDuration(ExitVehicleAnimation);
}

void AGrandCityMobileCharacter::CancelVehicleAnimation()
{
    if (HasAuthority())
    {
        MulticastCancelVehicleAnimation();
    }
}

void AGrandCityMobileCharacter::MulticastPlayEnterVehicle_Implementation(
    FVector_NetQuantize10 StartLocation,
    float StartYaw,
    float SeatHeight)
{
    BeginVehicleTransition(EGrandCityVehicleTransition::Entering);
    VehicleSeatHeight = SeatHeight;

    // Slide from wherever the player stood to the clip's start pose beside the door.
    bVehicleAligning = true;
    VehicleAlignElapsed = 0.0f;
    VehicleAlignFromLocation = GetActorLocation();
    VehicleAlignFromRotation = GetActorQuat();
    VehicleAlignToLocation = StartLocation;
    VehicleAlignToRotation = FRotator(0.0f, StartYaw, 0.0f).Quaternion();

    // Hold the final seated pose until the server seats the character.
    if (PlayVehicleMontage(EnterVehicleAnimation, false, VehicleMontageBlendTime) <= 0.0f && !HasAuthority())
    {
        EndVehicleTransition(true);
    }
}

void AGrandCityMobileCharacter::MulticastPlayExitVehicle_Implementation(
    FVector_NetQuantize10 SeatLocation,
    float SeatYaw)
{
    if (!HasAuthority())
    {
        // The detach replication may still be in flight on this client.
        DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    }

    BeginVehicleTransition(EGrandCityVehicleTransition::Exiting);
    ExitSeatLocation = SeatLocation;
    ExitSeatYaw = SeatYaw;
    TickVehicleTransition(0.0f);

    // The held seated pose is this clip's first frame, so swap to it without a blend.
    SeatedMontage = nullptr;
    const float Duration = PlayVehicleMontage(ExitVehicleAnimation, true, 0.0f);
    if (Duration <= 0.0f)
    {
        FinishExitVehicleAnimation();
        return;
    }

    // TickVehicleTransition hands control back once the clip has fully blended out
    // (sliding the mesh root from the seat to the capsule meanwhile). The timer is
    // only a fallback for machines that don't evaluate the animation.
    GetWorldTimerManager().SetTimer(
        VehicleTransitionTimer,
        this,
        &AGrandCityMobileCharacter::FinishExitVehicleAnimation,
        Duration + VehicleMontageBlendTime,
        false);
}

void AGrandCityMobileCharacter::MulticastCancelVehicleAnimation_Implementation()
{
    EndVehicleTransition(true);
}

void AGrandCityMobileCharacter::BeginVehicleTransition(EGrandCityVehicleTransition Transition)
{
    GetWorldTimerManager().ClearTimer(VehicleTransitionTimer);
    VehicleTransition = Transition;
    bVehicleAligning = false;
    bExitHandOffAnchored = false;
    // Forget any finished montage so the exit hand-off never reads a stale one.
    ActiveVehicleMontage = nullptr;
    bIsSprinting = false;
    ForwardInputValue = 0.0f;
    RightInputValue = 0.0f;
    ConsumeMovementInputVector();

    if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
    {
        MovementComponent->StopMovementImmediately();
        MovementComponent->DisableMovement();
    }
    SetActorEnableCollision(false);
}

void AGrandCityMobileCharacter::FinishExitVehicleAnimation()
{
    EndVehicleTransition(false);
}

void AGrandCityMobileCharacter::EndVehicleTransition(bool bStopMontage)
{
    GetWorldTimerManager().ClearTimer(VehicleTransitionTimer);
    const EGrandCityVehicleTransition PreviousTransition = VehicleTransition;
    VehicleTransition = EGrandCityVehicleTransition::None;
    bVehicleAligning = false;

    if (PreviousTransition == EGrandCityVehicleTransition::Exiting && GetMesh())
    {
        // The exit hand-off ends with the mesh root a couple of cm from its default spot
        // (the idle pelvis isn't exactly over the root). Shift the capsule by that residual
        // instead, so restoring the default mesh placement doesn't move the body.
        const FVector DefaultRootLocation = GetActorLocation() + GetActorQuat().RotateVector(DefaultMeshRelativeLocation);
        FVector Residual = GetMesh()->GetComponentLocation() - DefaultRootLocation;
        Residual.Z = 0.0f;
        constexpr float MaxResidual = 25.0f;
        if (Residual.SizeSquared() < FMath::Square(MaxResidual))
        {
            AddActorWorldOffset(Residual, false, nullptr, ETeleportType::TeleportPhysics);
        }
    }

    if (PreviousTransition != EGrandCityVehicleTransition::None)
    {
        RestoreDefaultMeshPlacement();
    }

    if (bStopMontage && ActiveVehicleMontage)
    {
        if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
        {
            AnimInstance->Montage_Stop(0.0f, ActiveVehicleMontage);
        }
        ActiveVehicleMontage = nullptr;
    }

    if (PreviousTransition != EGrandCityVehicleTransition::None)
    {
        // Restores collision and walking unless the character is now seated.
        ApplyOccupiedVehicleState();
    }
}

void AGrandCityMobileCharacter::TickVehicleTransition(float DeltaSeconds)
{
    // Where the mesh root sits with the default offset: on the ground under the capsule.
    const FVector DefaultRootLocation = GetActorLocation() + GetActorQuat().RotateVector(DefaultMeshRelativeLocation);

    if (VehicleTransition == EGrandCityVehicleTransition::Entering)
    {
        if (bVehicleAligning)
        {
            VehicleAlignElapsed += DeltaSeconds;
            const float Alpha = VehicleAlignDuration > KINDA_SMALL_NUMBER
                ? FMath::Clamp(VehicleAlignElapsed / VehicleAlignDuration, 0.0f, 1.0f)
                : 1.0f;
            const float SmoothAlpha = FMath::SmoothStep(0.0f, 1.0f, Alpha);
            SetActorLocationAndRotation(
                FMath::Lerp(VehicleAlignFromLocation, VehicleAlignToLocation, SmoothAlpha),
                FQuat::Slerp(VehicleAlignFromRotation, VehicleAlignToRotation, SmoothAlpha));
            bVehicleAligning = Alpha < 1.0f;
        }

        // The clip assumes the seat is at ground level; lift the body to the real seat
        // height while the character climbs in, so the final pose matches the seated pose.
        const float StepAlpha = GetVehicleStepAlpha(EnterVehicleAnimation, EnterAnimationStepWindow);
        const FVector GroundRoot = GetActorLocation() + GetActorQuat().RotateVector(DefaultMeshRelativeLocation);
        SetMeshWorldPlacement(
            GroundRoot + FVector(0.0f, 0.0f, (VehicleSeatHeight - GroundRoot.Z) * StepAlpha),
            GetActorQuat() * DefaultMeshRelativeRotation.Quaternion());
    }
    else if (VehicleTransition == EGrandCityVehicleTransition::Exiting)
    {
        // The capsule already waits at the exit spot (so the camera doesn't jump when the
        // clip ends) and the mesh root is pinned to the seat so the clip plays from inside
        // the car. Re-evaluated every tick because clients may receive the capsule late.
        const UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
        const FAnimMontageInstance* MontageInstance = AnimInstance && ActiveVehicleMontage
            ? AnimInstance->GetInstanceForMontage(ActiveVehicleMontage)
            : nullptr;

        if (ActiveVehicleMontage && !MontageInstance)
        {
            // Fully blended out: HandleBoneTransformsFinalized already stood the body
            // over the capsule, so the default placement continues seamlessly.
            FinishExitVehicleAnimation();
            return;
        }

        if (MontageInstance && MontageInstance->IsStopped())
        {
            // Blending out: HandleBoneTransformsFinalized places the mesh after each pose
            // evaluation, because this tick may run before or after the animation update.
            return;
        }

        // Pinned to the seat, stepping down to the ground while the character climbs out.
        const float StepAlpha = GetVehicleStepAlpha(ExitVehicleAnimation, ExitAnimationStepWindow);
        FVector RootLocation = ExitSeatLocation;
        RootLocation.Z = FMath::Lerp(ExitSeatLocation.Z, DefaultRootLocation.Z, StepAlpha);
        SetMeshWorldPlacement(
            RootLocation,
            FRotator(0.0f, ExitSeatYaw + DefaultMeshRelativeRotation.Yaw, 0.0f).Quaternion());
    }
}

void AGrandCityMobileCharacter::HandleBoneTransformsFinalized()
{
    if (VehicleTransition != EGrandCityVehicleTransition::Exiting || !ActiveVehicleMontage)
    {
        return;
    }

    USkeletalMeshComponent* MeshComponent = GetMesh();
    const UAnimInstance* AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;
    if (!AnimInstance)
    {
        return;
    }

    const FAnimMontageInstance* MontageInstance = AnimInstance->GetInstanceForMontage(ActiveVehicleMontage);
    if (MontageInstance && !MontageInstance->IsStopped())
    {
        // Still playing: TickVehicleTransition keeps the root pinned to the seat.
        return;
    }

    const int32 PelvisIndex = MeshComponent->GetBoneIndex(TEXT("pelvis"));
    if (PelvisIndex == INDEX_NONE)
    {
        return;
    }

    // Hand-off. The clip has no root motion, so its last pose keeps the pelvis ~1.8 m from
    // the animation root and turned 90 degrees, while locomotion keeps it over the root.
    // Using the pose that was just evaluated, turn the mesh root with the blend weight and
    // move it so the pelvis glides from where the clip left it to where it will rest once
    // the default mesh placement is restored.
    const FVector PelvisComponent = MeshComponent->GetBoneTransform(PelvisIndex, FTransform::Identity).GetLocation();
    if (!bExitHandOffAnchored)
    {
        ExitHandOffPelvisAnchor = MeshComponent->GetComponentTransform().TransformPosition(PelvisComponent);
        bExitHandOffAnchored = true;
    }

    const float HandOffAlpha = MontageInstance ? 1.0f - MontageInstance->GetWeight() : 1.0f;
    const FQuat ActorRotation = GetActorQuat();
    const FVector DefaultRootLocation = GetActorLocation() + ActorRotation.RotateVector(DefaultMeshRelativeLocation);
    const FQuat DefaultRootRotation = ActorRotation * DefaultMeshRelativeRotation.Quaternion();
    const FQuat SeatRootRotation = FRotator(0.0f, ExitSeatYaw + DefaultMeshRelativeRotation.Yaw, 0.0f).Quaternion();
    const FQuat RootRotation = FQuat::Slerp(SeatRootRotation, DefaultRootRotation, HandOffAlpha);

    // The pelvis comes to rest over the capsule: the idle pose keeps it within a few cm of
    // the root. (It must not be derived from the pose being blended, whose pelvis is still
    // up to ~1.8 m from the root mid-blend.)
    const FVector PelvisTarget = FMath::Lerp(ExitHandOffPelvisAnchor, DefaultRootLocation, HandOffAlpha);
    const FVector PelvisOffset = RootRotation.RotateVector(PelvisComponent);
    // The idle pose's small pelvis offset leaves the root a couple of cm off its default
    // spot when the blend completes; EndVehicleTransition absorbs that by moving the capsule.
    SetMeshWorldPlacement(
        FVector(PelvisTarget.X - PelvisOffset.X, PelvisTarget.Y - PelvisOffset.Y, DefaultRootLocation.Z),
        RootRotation);
}

float AGrandCityMobileCharacter::GetVehicleStepAlpha(const UAnimSequence* Animation, const FVector2D& Window) const
{
    const UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
    if (!Animation || !AnimInstance || !ActiveVehicleMontage)
    {
        return 0.0f;
    }

    const FAnimMontageInstance* MontageInstance = AnimInstance->GetInstanceForMontage(ActiveVehicleMontage);
    if (!MontageInstance)
    {
        return 1.0f;
    }

    const float Fraction = MontageInstance->GetPosition() / FMath::Max(Animation->GetPlayLength(), KINDA_SMALL_NUMBER);
    const float WindowStart = static_cast<float>(Window.X);
    const float WindowEnd = FMath::Max(static_cast<float>(Window.Y), WindowStart + KINDA_SMALL_NUMBER);
    return FMath::SmoothStep(WindowStart, WindowEnd, Fraction);
}

void AGrandCityMobileCharacter::SetMeshWorldPlacement(const FVector& RootLocation, const FQuat& RootRotation)
{
    USkeletalMeshComponent* MeshComponent = GetMesh();
    if (!MeshComponent)
    {
        return;
    }

    const FQuat ActorRotation = GetActorQuat();
    const FVector RelativeLocation = ActorRotation.UnrotateVector(RootLocation - GetActorLocation());
    const FRotator RelativeRotation = (ActorRotation.Inverse() * RootRotation).Rotator();
    MeshComponent->SetRelativeLocationAndRotation(RelativeLocation, RelativeRotation);
    // Network smoothing on simulated proxies re-applies this base offset every frame.
    CacheInitialMeshOffset(RelativeLocation, RelativeRotation);
}

void AGrandCityMobileCharacter::RestoreDefaultMeshPlacement()
{
    if (USkeletalMeshComponent* MeshComponent = GetMesh())
    {
        MeshComponent->SetRelativeLocationAndRotation(DefaultMeshRelativeLocation, DefaultMeshRelativeRotation);
        CacheInitialMeshOffset(DefaultMeshRelativeLocation, DefaultMeshRelativeRotation);
    }
}

void AGrandCityMobileCharacter::PlaySeatedPose()
{
    UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
    if (!AnimInstance || !ExitVehicleAnimation
        || (SeatedMontage && AnimInstance->Montage_IsActive(SeatedMontage)))
    {
        return;
    }

    // No blend-in: this replaces the enter clip's final (seated) frame, and blending two
    // poses whose roots sit in different places would slide the body around.
    UAnimMontage* Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
        ExitVehicleAnimation, CrouchAnimationSlotName, 0.0f, VehicleMontageBlendTime, 1.0f, 1);
    if (!Montage)
    {
        return;
    }

    Montage->bEnableAutoBlendOut = false;
    if (AnimInstance->Montage_Play(Montage) <= 0.0f)
    {
        return;
    }

    AnimInstance->Montage_Pause(Montage);
    ActiveCrouchMontage = nullptr;
    ActiveCrouchAnimation = nullptr;
    ActiveVehicleMontage = nullptr;
    SeatedMontage = Montage;
}

void AGrandCityMobileCharacter::StopSeatedPose()
{
    if (!SeatedMontage)
    {
        return;
    }

    if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
    {
        AnimInstance->Montage_Stop(VehicleMontageBlendTime, SeatedMontage);
    }
    SeatedMontage = nullptr;
}

float AGrandCityMobileCharacter::PlayVehicleMontage(UAnimSequence* Animation, bool bAutoBlendOut, float BlendInTime)
{
    UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
    if (!AnimInstance || !Animation)
    {
        return 0.0f;
    }

    UAnimMontage* Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
        Animation,
        CrouchAnimationSlotName,
        BlendInTime,
        VehicleMontageBlendTime,
        1.0f,
        1);
    if (!Montage)
    {
        return 0.0f;
    }

    Montage->bEnableAutoBlendOut = bAutoBlendOut;
    // CreateSlotAnimationAsDynamicMontage ignores its play-rate argument (UE 5.8), so the
    // rate must go to Montage_Play; the enter/exit timers assume this exact duration.
    if (AnimInstance->Montage_Play(Montage, FMath::Max(VehicleAnimationPlayRate, 0.1f)) <= 0.0f)
    {
        return 0.0f;
    }

    // Montage_Play replaced any crouch montage in the shared slot.
    ActiveCrouchMontage = nullptr;
    ActiveCrouchAnimation = nullptr;
    ActiveVehicleMontage = Montage;
    return GetVehicleAnimationDuration(Animation);
}

float AGrandCityMobileCharacter::GetVehicleAnimationDuration(const UAnimSequence* Animation) const
{
    return Animation ? Animation->GetPlayLength() / FMath::Max(VehicleAnimationPlayRate, 0.1f) : 0.0f;
}

bool AGrandCityMobileCharacter::FindVehicleStandSpot(
    const AGrandCityVehicle* Vehicle,
    const FVector& DesiredFeetLocation,
    FVector& OutActorLocation) const
{
    const UWorld* World = GetWorld();
    const UCapsuleComponent* Capsule = GetCapsuleComponent();
    if (!World || !Capsule || !Vehicle)
    {
        return false;
    }

    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(GrandCityVehicleStandSpot), false, this);

    // Needs real floor under it; the car's own roof or hood doesn't count.
    FHitResult FloorHit;
    const FVector TraceStart = DesiredFeetLocation + FVector(0.0f, 0.0f, HalfHeight * 2.0f);
    const FVector TraceEnd = DesiredFeetLocation - FVector(0.0f, 0.0f, 150.0f);
    if (!World->LineTraceSingleByChannel(FloorHit, TraceStart, TraceEnd, ECC_Pawn, Params)
        || FloorHit.GetActor() == Vehicle)
    {
        return false;
    }

    const FVector ActorLocation = FloorHit.ImpactPoint + FVector(0.0f, 0.0f, HalfHeight + 2.0f);
    if (World->OverlapBlockingTestByChannel(
        ActorLocation, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params))
    {
        return false;
    }

    // The clip walks straight between the seat and this spot, so no wall may be in between.
    FCollisionQueryParams SightParams(Params);
    SightParams.AddIgnoredActor(Vehicle);
    const FVector SeatLocation = Vehicle->GetDriverSeatTransform().GetLocation() + FVector(0.0f, 0.0f, HalfHeight);
    FHitResult WallHit;
    if (World->LineTraceSingleByChannel(WallHit, SeatLocation, ActorLocation, ECC_Pawn, SightParams))
    {
        return false;
    }

    OutActorLocation = ActorLocation;
    return true;
}

void AGrandCityMobileCharacter::MoveForward(float Value)
{
    ForwardInputValue = Value;
    if (!bCrouchMovementPaused && FMath::Abs(Value) > KINDA_SMALL_NUMBER
        && (CrouchAnimationPhase == EGrandCityCrouchAnimationPhase::Entering
            || CrouchAnimationPhase == EGrandCityCrouchAnimationPhase::Exiting))
    {
        const float RemainingAnimationTime = FMath::Max(
            GetWorldTimerManager().GetTimerRemaining(CrouchAnimationTimer), 0.0f);
        PauseMovementForCrouchTransition(RemainingAnimationTime + CrouchMovementPauseDuration);
    }
    if (bCrouchMovementPaused)
    {
        return;
    }

    if (Controller && FMath::Abs(Value) > KINDA_SMALL_NUMBER)
    {
        const FRotator Rotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
        AddMovementInput(FRotationMatrix(Rotation).GetUnitAxis(EAxis::X), Value);
    }
}

void AGrandCityMobileCharacter::MoveRight(float Value)
{
    RightInputValue = Value;
    if (!bCrouchMovementPaused && FMath::Abs(Value) > KINDA_SMALL_NUMBER
        && (CrouchAnimationPhase == EGrandCityCrouchAnimationPhase::Entering
            || CrouchAnimationPhase == EGrandCityCrouchAnimationPhase::Exiting))
    {
        const float RemainingAnimationTime = FMath::Max(
            GetWorldTimerManager().GetTimerRemaining(CrouchAnimationTimer), 0.0f);
        PauseMovementForCrouchTransition(RemainingAnimationTime + CrouchMovementPauseDuration);
    }
    if (bCrouchMovementPaused)
    {
        return;
    }

    if (Controller && FMath::Abs(Value) > KINDA_SMALL_NUMBER)
    {
        const FRotator Rotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
        AddMovementInput(FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y), Value);
    }
}

void AGrandCityMobileCharacter::Turn(float Value)
{
    AddControllerYawInput(Value);
}

void AGrandCityMobileCharacter::LookUp(float Value)
{
    AddControllerPitchInput(Value);
}

void AGrandCityMobileCharacter::StartSprint()
{
    const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
    if (bIsSprinting || bIsCrouched || (MovementComponent && MovementComponent->bWantsToCrouch)
        || IsInVehicleTransition())
    {
        return;
    }

    bIsSprinting = true;
    ApplyMovementSpeed();

    if (!HasAuthority())
    {
        ServerSetSprinting(true);
    }
}

void AGrandCityMobileCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
    Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

    GetWorldTimerManager().ClearTimer(CrouchAnimationTimer);

    // A quick reversal blends straight back to crouch without restarting Entry.
    if (CrouchAnimationPhase == EGrandCityCrouchAnimationPhase::Exiting)
    {
        BeginCrouchLoop();
        return;
    }

    if (CrouchEntryAnimation)
    {
        const float EntryLength = FMath::Max(0.05f, CrouchEntryAnimation->GetPlayLength());
        if (IsMovingForCrouchTransition())
        {
            PauseMovementForCrouchTransition(EntryLength + CrouchMovementPauseDuration);
        }
        CrouchAnimationPhase = EGrandCityCrouchAnimationPhase::Entering;
        PlayCrouchAnimation(CrouchEntryAnimation, false);
        GetWorldTimerManager().SetTimer(
            CrouchAnimationTimer,
            this,
            &AGrandCityMobileCharacter::BeginCrouchLoop,
            EntryLength,
            false);
    }
    else
    {
        BeginCrouchLoop();
    }
}

void AGrandCityMobileCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
    Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

    GetWorldTimerManager().ClearTimer(CrouchAnimationTimer);

    // A quick reversal blends straight back to standing without restarting Exit.
    if (CrouchAnimationPhase == EGrandCityCrouchAnimationPhase::Entering)
    {
        RestoreStandingAnimation();
        return;
    }

    if (CrouchExitAnimation)
    {
        const float ExitLength = FMath::Max(0.05f, CrouchExitAnimation->GetPlayLength());
        if (IsMovingForCrouchTransition())
        {
            PauseMovementForCrouchTransition(ExitLength + CrouchMovementPauseDuration);
        }
        CrouchAnimationPhase = EGrandCityCrouchAnimationPhase::Exiting;
        PlayCrouchAnimation(CrouchExitAnimation, false);
        GetWorldTimerManager().SetTimer(
            CrouchAnimationTimer,
            this,
            &AGrandCityMobileCharacter::RestoreStandingAnimation,
            ExitLength,
            false);
    }
    else
    {
        RestoreStandingAnimation();
    }
}

void AGrandCityMobileCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(VehicleTransitionTimer);
    if (USkeletalMeshComponent* MeshComponent = GetMesh())
    {
        MeshComponent->UnregisterOnBoneTransformsFinalizedDelegate(BoneTransformsFinalizedHandle);
    }
    GetWorldTimerManager().ClearTimer(CrouchAnimationTimer);
    GetWorldTimerManager().ClearTimer(CrouchMovementPauseTimer);
    Super::EndPlay(EndPlayReason);
}

void AGrandCityMobileCharacter::StopSprint()
{
    bIsSprinting = false;
    ApplyMovementSpeed();

    if (!HasAuthority())
    {
        ServerSetSprinting(false);
    }
}

void AGrandCityMobileCharacter::ToggleSprint()
{
    if (bIsSprinting)
    {
        StopSprint();
    }
    else
    {
        StartSprint();
    }
}

void AGrandCityMobileCharacter::ToggleCrouch()
{
    UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
    if (!MovementComponent || IsInVehicleTransition())
    {
        return;
    }

    if (IsMovingForCrouchTransition())
    {
        PauseMovementForCrouchTransition(CrouchMovementPauseDuration);
    }

    if (bIsCrouched || MovementComponent->bWantsToCrouch)
    {
        UnCrouch();
        return;
    }

    StopSprint();
    Crouch();
}

bool AGrandCityMobileCharacter::WantsToCrouch() const
{
    const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
    return MovementComponent ? MovementComponent->bWantsToCrouch : bIsCrouched;
}

void AGrandCityMobileCharacter::BeginCrouchLoop()
{
    GetWorldTimerManager().ClearTimer(CrouchAnimationTimer);

#if !UE_BUILD_SHIPPING
    if (bCrouchMovementPlaytestEnabled
        && CrouchAnimationPhase == EGrandCityCrouchAnimationPhase::Entering)
    {
        UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
        const FAnimMontageInstance* EntryInstance = AnimInstance && ActiveCrouchMontage
            ? AnimInstance->GetActiveInstanceForMontage(ActiveCrouchMontage)
            : nullptr;
        const bool bHoldingEntryPose = EntryInstance && !EntryInstance->bEnableAutoBlendOut;
        bCrouchMovementPlaytestFailed |= !bHoldingEntryPose;
        UE_LOG(LogGrandCityCrouchPlaytest, Display,
            TEXT("ENTRY_HANDOFF hold_pose=%d active_montage=%d"),
            bHoldingEntryPose, EntryInstance != nullptr);
    }
#endif

    if (!bIsCrouched)
    {
        RestoreStandingAnimation();
        return;
    }

    const bool bWasMoving = IsMovingForCrouchTransition();
    CrouchAnimationPhase = EGrandCityCrouchAnimationPhase::Crouched;
    ActiveCrouchAnimation = nullptr;
    UpdateCrouchLocomotion();
    if (bWasMoving)
    {
        PauseMovementForCrouchTransition(CrouchMovementPauseDuration);
    }
}

void AGrandCityMobileCharacter::RestoreStandingAnimation()
{
    GetWorldTimerManager().ClearTimer(CrouchAnimationTimer);
    const bool bWasMoving = IsMovingForCrouchTransition();
    CrouchAnimationPhase = EGrandCityCrouchAnimationPhase::Standing;

    if (USkeletalMeshComponent* MeshComponent = GetMesh())
    {
        if (UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance())
        {
            if (ActiveCrouchMontage)
            {
                AnimInstance->Montage_Stop(CrouchAnimationBlendTime, ActiveCrouchMontage);
            }
        }
    }

    ActiveCrouchAnimation = nullptr;
    ActiveCrouchMontage = nullptr;
    if (bWasMoving)
    {
        PauseMovementForCrouchTransition(CrouchMovementPauseDuration);
    }
}

void AGrandCityMobileCharacter::UpdateCrouchLocomotion()
{
    UAnimSequence* DesiredAnimation = SelectCrouchLocomotionAnimation();
    if (!DesiredAnimation)
    {
        return;
    }

    const float HorizontalSpeed = GetVelocity().Size2D();
    const bool bIsMoving = HorizontalSpeed > 5.0f;
    const float MovementPlayRate = bIsMoving
        ? FMath::Clamp(HorizontalSpeed / FMath::Max(CrouchSpeed, 1.0f), 0.65f, 1.25f)
        : 1.0f;

    PlayCrouchAnimation(DesiredAnimation, true, MovementPlayRate);
}

void AGrandCityMobileCharacter::PlayCrouchAnimation(
    UAnimSequence* Animation,
    bool bLooping,
    float PlayRate)
{
    USkeletalMeshComponent* MeshComponent = GetMesh();
    if (!MeshComponent || !Animation)
    {
        return;
    }

    UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance();
    if (!AnimInstance)
    {
        return;
    }

    if (ActiveCrouchAnimation != Animation
        || !ActiveCrouchMontage
        || !AnimInstance->Montage_IsPlaying(ActiveCrouchMontage))
    {
        UAnimMontage* NewMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
            Animation,
            CrouchAnimationSlotName,
            CrouchAnimationBlendTime,
            CrouchAnimationBlendTime,
            PlayRate,
            bLooping ? CrouchAnimationLoopCount : 1);
        if (!NewMontage)
        {
            return;
        }

        if (!bLooping)
        {
            // Hold Entry/Exit's final pose until the timer starts the next montage.
            // Automatic blend-out exposes the standing AnimBP before that handoff.
            NewMontage->bEnableAutoBlendOut = false;
        }

        if (AnimInstance->Montage_Play(NewMontage) <= 0.0f)
        {
            return;
        }

        ActiveCrouchAnimation = Animation;
        ActiveCrouchMontage = NewMontage;
    }
    else
    {
        AnimInstance->Montage_SetPlayRate(ActiveCrouchMontage, PlayRate);
    }
}

bool AGrandCityMobileCharacter::IsMovingForCrouchTransition() const
{
    const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
    return bCrouchMovementPaused
        || GetVelocity().SizeSquared2D() > FMath::Square(CrouchTransitionMoveSpeed)
        || (MovementComponent && !MovementComponent->GetCurrentAcceleration().IsNearlyZero())
        || FMath::Square(ForwardInputValue) + FMath::Square(RightInputValue) > 0.01f;
}

void AGrandCityMobileCharacter::PauseMovementForCrouchTransition(float DurationSeconds)
{
    UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
    if (!MovementComponent)
    {
        return;
    }

    // Clear momentum and pending input before locking both standing and crouched speeds.
#if !UE_BUILD_SHIPPING
    if (bCrouchMovementPlaytestEnabled && !bCrouchMovementPaused)
    {
        CrouchMovementPlaytestPauseStartLocation = GetActorLocation();
        ++CrouchMovementPlaytestPauseCount;
    }
#endif
    bCrouchMovementPaused = true;
    MovementComponent->StopMovementImmediately();
    ConsumeMovementInputVector();
    ApplyMovementSpeed();
#if !UE_BUILD_SHIPPING
    if (bCrouchMovementPlaytestEnabled)
    {
        UE_LOG(LogGrandCityCrouchPlaytest, Display,
            TEXT("PAUSE count=%d phase=%d velocity=%.3f walk=%.3f crouch=%.3f duration=%.3f"),
            CrouchMovementPlaytestPauseCount, static_cast<int32>(CrouchAnimationPhase),
            GetVelocity().Size2D(), MovementComponent->MaxWalkSpeed,
            MovementComponent->MaxWalkSpeedCrouched, DurationSeconds);
    }
#endif
    if (ActiveCrouchMontage)
    {
        if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
        {
            AnimInstance->Montage_SetPlayRate(ActiveCrouchMontage, 1.0f);
        }
    }

    GetWorldTimerManager().SetTimer(
        CrouchMovementPauseTimer,
        this,
        &AGrandCityMobileCharacter::ResumeMovementAfterCrouchTransition,
        FMath::Max(DurationSeconds, CrouchMovementPauseDuration),
        false);
}

void AGrandCityMobileCharacter::ResumeMovementAfterCrouchTransition()
{
    GetWorldTimerManager().ClearTimer(CrouchMovementPauseTimer);

#if !UE_BUILD_SHIPPING
    if (bCrouchMovementPlaytestEnabled)
    {
        CrouchMovementPlaytestMaxPauseDrift = FMath::Max(
            CrouchMovementPlaytestMaxPauseDrift,
            FVector::Dist2D(GetActorLocation(), CrouchMovementPlaytestPauseStartLocation));
        if (CrouchMovementPlaytestMaxPauseDrift > 1.0f)
        {
            bCrouchMovementPlaytestFailed = true;
        }
        ++CrouchMovementPlaytestCompletedPauses;
    }
#endif
    bCrouchMovementPaused = false;
    ApplyMovementSpeed();

#if !UE_BUILD_SHIPPING
    if (bCrouchMovementPlaytestEnabled)
    {
        const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
        UE_LOG(LogGrandCityCrouchPlaytest, Display,
            TEXT("RESUME count=%d phase=%d drift=%.3f walk=%.3f crouch=%.3f"),
            CrouchMovementPlaytestCompletedPauses, static_cast<int32>(CrouchAnimationPhase),
            CrouchMovementPlaytestMaxPauseDrift,
            MovementComponent ? MovementComponent->MaxWalkSpeed : -1.0f,
            MovementComponent ? MovementComponent->MaxWalkSpeedCrouched : -1.0f);
    }
#endif

    if (CrouchAnimationPhase == EGrandCityCrouchAnimationPhase::Crouched && bIsCrouched)
    {
        UpdateCrouchLocomotion();
    }
}

#if !UE_BUILD_SHIPPING
void AGrandCityMobileCharacter::TickCrouchMovementPlaytest()
{
    if (!bCrouchMovementPlaytestEnabled || !IsLocallyControlled() || !Controller
        || !Controller->IsPlayerController() || !GetWorld())
    {
        return;
    }

    const float Now = GetWorld()->GetTimeSeconds();
    if (CrouchMovementPlaytestStageStartTime < 0.0f)
    {
        CrouchMovementPlaytestStageStartTime = Now;
        CrouchMovementPlaytestStageStartLocation = GetActorLocation();
        UE_LOG(LogGrandCityCrouchPlaytest, Display, TEXT("START pawn=%s"), *GetName());
    }

    MoveForward(1.0f);
    if (bCrouchMovementPaused)
    {
        CrouchMovementPlaytestMaxPauseDrift = FMath::Max(
            CrouchMovementPlaytestMaxPauseDrift,
            FVector::Dist2D(GetActorLocation(), CrouchMovementPlaytestPauseStartLocation));
    }

    const float StageElapsed = Now - CrouchMovementPlaytestStageStartTime;
    if (CrouchMovementPlaytestStage == 0 && StageElapsed >= 1.0f)
    {
        const float Distance = FVector::Dist2D(
            GetActorLocation(), CrouchMovementPlaytestStageStartLocation);
        bCrouchMovementPlaytestFailed |= Distance < 20.0f;
        UE_LOG(LogGrandCityCrouchPlaytest, Display,
            TEXT("TOGGLE_CROUCH pretravel=%.3f velocity=%.3f"), Distance, GetVelocity().Size2D());
        CrouchMovementPlaytestStage = 1;
        CrouchMovementPlaytestStageStartTime = Now;
        ToggleCrouch();
    }
    else if (CrouchMovementPlaytestStage == 1
        && CrouchMovementPlaytestCompletedPauses >= 1
        && CrouchAnimationPhase == EGrandCityCrouchAnimationPhase::Crouched)
    {
        CrouchMovementPlaytestStage = 2;
        CrouchMovementPlaytestStageStartTime = Now;
        CrouchMovementPlaytestStageStartLocation = GetActorLocation();
    }
    else if (CrouchMovementPlaytestStage == 2 && StageElapsed >= 0.6f)
    {
        const float Distance = FVector::Dist2D(
            GetActorLocation(), CrouchMovementPlaytestStageStartLocation);
        bCrouchMovementPlaytestFailed |= Distance < 20.0f;
        UE_LOG(LogGrandCityCrouchPlaytest, Display,
            TEXT("TOGGLE_STAND crouch_travel=%.3f velocity=%.3f"), Distance, GetVelocity().Size2D());
        CrouchMovementPlaytestStage = 3;
        CrouchMovementPlaytestStageStartTime = Now;
        ToggleCrouch();
    }
    else if (CrouchMovementPlaytestStage == 3
        && CrouchMovementPlaytestCompletedPauses >= 2
        && CrouchAnimationPhase == EGrandCityCrouchAnimationPhase::Standing)
    {
        CrouchMovementPlaytestStage = 4;
        CrouchMovementPlaytestStageStartTime = Now;
        CrouchMovementPlaytestStageStartLocation = GetActorLocation();
    }
    else if (CrouchMovementPlaytestStage == 4 && StageElapsed >= 0.6f)
    {
        const float Distance = FVector::Dist2D(
            GetActorLocation(), CrouchMovementPlaytestStageStartLocation);
        const bool bPassed = !bCrouchMovementPlaytestFailed && Distance >= 20.0f
            && CrouchMovementPlaytestPauseCount >= 2
            && CrouchMovementPlaytestCompletedPauses >= 2
            && !bCrouchMovementPaused;
        UE_LOG(LogGrandCityCrouchPlaytest, Display,
            TEXT("%s pauses=%d completed=%d max_drift=%.3f standing_travel=%.3f"),
            bPassed ? TEXT("PASS") : TEXT("FAIL"), CrouchMovementPlaytestPauseCount,
            CrouchMovementPlaytestCompletedPauses, CrouchMovementPlaytestMaxPauseDrift, Distance);
        CrouchMovementPlaytestStage = 5;
        FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
    }

    if (CrouchMovementPlaytestStage != 5
        && Now - CrouchMovementPlaytestStageStartTime > 10.0f)
    {
        UE_LOG(LogGrandCityCrouchPlaytest, Error,
            TEXT("FAIL timeout stage=%d pauses=%d completed=%d"),
            CrouchMovementPlaytestStage, CrouchMovementPlaytestPauseCount,
            CrouchMovementPlaytestCompletedPauses);
        CrouchMovementPlaytestStage = 5;
        FPlatformMisc::RequestExitWithStatus(false, 1);
    }
}
#endif

UAnimSequence* AGrandCityMobileCharacter::SelectCrouchLocomotionAnimation() const
{
    FVector MovementDirectionSource(GetVelocity().X, GetVelocity().Y, 0.0f);
    const bool bWasMoving = ActiveCrouchAnimation
        && ActiveCrouchAnimation != CrouchIdleAnimation;
    const float MovementThreshold = bWasMoving ? 8.0f : 20.0f;
    if (MovementDirectionSource.SizeSquared() <= FMath::Square(MovementThreshold))
    {
        MovementDirectionSource = FVector::ZeroVector;
        if (Controller)
        {
            const FRotator Rotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
            const FRotationMatrix InputRotation(Rotation);
            MovementDirectionSource =
                InputRotation.GetUnitAxis(EAxis::X) * ForwardInputValue
                + InputRotation.GetUnitAxis(EAxis::Y) * RightInputValue;
        }
        if (MovementDirectionSource.IsNearlyZero())
        {
            const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
            const FVector Acceleration = MovementComponent
                ? MovementComponent->GetCurrentAcceleration()
                : FVector::ZeroVector;
            MovementDirectionSource = FVector(Acceleration.X, Acceleration.Y, 0.0f);
        }
        if (MovementDirectionSource.IsNearlyZero())
        {
            return CrouchIdleAnimation;
        }
    }

    const FVector MovementDirection = MovementDirectionSource.GetSafeNormal();
    const float ForwardAmount = FVector::DotProduct(GetActorForwardVector(), MovementDirection);
    const float RightAmount = FVector::DotProduct(GetActorRightVector(), MovementDirection);

    if (FMath::Abs(ForwardAmount) >= FMath::Abs(RightAmount))
    {
        return ForwardAmount >= 0.0f
            ? CrouchWalkForwardAnimation
            : CrouchWalkBackwardAnimation;
    }

    return RightAmount >= 0.0f
        ? CrouchWalkRightAnimation
        : CrouchWalkLeftAnimation;
}

void AGrandCityMobileCharacter::ServerSetSprinting_Implementation(bool bNewSprinting)
{
    const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
    bIsSprinting = bNewSprinting
        && !bIsCrouched
        && (!MovementComponent || !MovementComponent->bWantsToCrouch);
    ApplyMovementSpeed();
}

void AGrandCityMobileCharacter::OnRep_Sprinting()
{
    ApplyMovementSpeed();
}

void AGrandCityMobileCharacter::ApplyMovementSpeed()
{
    if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
    {
        MovementComponent->MaxWalkSpeed = bCrouchMovementPaused
            ? 0.0f
            : (bIsSprinting ? SprintSpeed : WalkSpeed);
        MovementComponent->MaxWalkSpeedCrouched = bCrouchMovementPaused ? 0.0f : CrouchSpeed;
    }
}
