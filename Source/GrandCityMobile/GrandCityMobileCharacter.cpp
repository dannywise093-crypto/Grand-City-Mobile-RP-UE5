#include "GrandCityMobileCharacter.h"
#include "Vehicles/GrandCityVehicle.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
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
    }
}

void AGrandCityMobileCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

#if !UE_BUILD_SHIPPING
    TickCrouchMovementPlaytest();
#endif

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
    // Ride along hidden inside the vehicle so the character's replicated
    // location stays with the car (relevancy, profile saves, exit fallback).
    AttachToActor(Vehicle, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
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
    SetActorHiddenInGame(bInVehicle);
    SetActorEnableCollision(!bInVehicle);
    ApplyMovementSpeed();

    if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
    {
        if (bInVehicle)
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
    if (bIsSprinting || bIsCrouched || (MovementComponent && MovementComponent->bWantsToCrouch))
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
    if (!MovementComponent)
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
