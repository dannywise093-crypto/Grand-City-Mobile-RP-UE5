#include "GrandCityMobileCharacter.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/InputComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

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

    if (USkeletalMeshComponent* MeshComponent = GetMesh())
    {
        StandingAnimInstanceClass = MeshComponent->GetAnimClass();
    }
}

void AGrandCityMobileCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

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
}

void AGrandCityMobileCharacter::MoveForward(float Value)
{
    if (Controller && FMath::Abs(Value) > KINDA_SMALL_NUMBER)
    {
        const FRotator Rotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
        AddMovementInput(FRotationMatrix(Rotation).GetUnitAxis(EAxis::X), Value);
    }
}

void AGrandCityMobileCharacter::MoveRight(float Value)
{
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

    if (CrouchEntryAnimation)
    {
        CrouchAnimationPhase = EGrandCityCrouchAnimationPhase::Entering;
        PlayCrouchAnimation(CrouchEntryAnimation, false);
        GetWorldTimerManager().SetTimer(
            CrouchAnimationTimer,
            this,
            &AGrandCityMobileCharacter::BeginCrouchLoop,
            FMath::Max(0.05f, CrouchEntryAnimation->GetPlayLength()),
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

    if (CrouchExitAnimation)
    {
        CrouchAnimationPhase = EGrandCityCrouchAnimationPhase::Exiting;
        PlayCrouchAnimation(CrouchExitAnimation, false);
        GetWorldTimerManager().SetTimer(
            CrouchAnimationTimer,
            this,
            &AGrandCityMobileCharacter::RestoreStandingAnimation,
            FMath::Max(0.05f, CrouchExitAnimation->GetPlayLength()),
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

    if (!bIsCrouched)
    {
        RestoreStandingAnimation();
        return;
    }

    CrouchAnimationPhase = EGrandCityCrouchAnimationPhase::Crouched;
    ActiveCrouchAnimation = nullptr;
    UpdateCrouchLocomotion();
}

void AGrandCityMobileCharacter::RestoreStandingAnimation()
{
    GetWorldTimerManager().ClearTimer(CrouchAnimationTimer);
    CrouchAnimationPhase = EGrandCityCrouchAnimationPhase::Standing;
    ActiveCrouchAnimation = nullptr;

    if (USkeletalMeshComponent* MeshComponent = GetMesh())
    {
        if (StandingAnimInstanceClass)
        {
            MeshComponent->SetAnimInstanceClass(StandingAnimInstanceClass);
        }
        else
        {
            MeshComponent->SetAnimationMode(EAnimationMode::AnimationBlueprint);
        }
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

    if (ActiveCrouchAnimation != Animation
        || MeshComponent->GetAnimationMode() != EAnimationMode::AnimationSingleNode)
    {
        MeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        MeshComponent->PlayAnimation(Animation, bLooping);
        ActiveCrouchAnimation = Animation;
    }

    if (UAnimSingleNodeInstance* SingleNodeInstance = MeshComponent->GetSingleNodeInstance())
    {
        SingleNodeInstance->SetPlaying(true);
        SingleNodeInstance->SetLooping(bLooping);
        SingleNodeInstance->SetPlayRate(PlayRate);
    }
}

UAnimSequence* AGrandCityMobileCharacter::SelectCrouchLocomotionAnimation() const
{
    const FVector HorizontalVelocity(GetVelocity().X, GetVelocity().Y, 0.0f);
    const bool bWasMoving = ActiveCrouchAnimation
        && ActiveCrouchAnimation != CrouchIdleAnimation;
    const float MovementThreshold = bWasMoving ? 8.0f : 20.0f;
    if (HorizontalVelocity.SizeSquared() <= FMath::Square(MovementThreshold))
    {
        return CrouchIdleAnimation;
    }

    const FVector MovementDirection = HorizontalVelocity.GetSafeNormal();
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
        MovementComponent->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
    }
}
