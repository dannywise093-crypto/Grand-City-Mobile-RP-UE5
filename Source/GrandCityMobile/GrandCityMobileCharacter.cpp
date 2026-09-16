#include "GrandCityMobileCharacter.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/InputComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Net/UnrealNetwork.h"
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
        GetMesh()->SetAnimInstanceClass(CharacterAnimation.Class);
    }

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
    GetCharacterMovement()->BrakingDecelerationWalking = 1800.0f;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
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
    if (bIsSprinting)
    {
        return;
    }

    if (HasAuthority())
    {
        bIsSprinting = true;
        ApplyMovementSpeed();
    }
    else
    {
        ServerSetSprinting(true);
    }
}

void AGrandCityMobileCharacter::StopSprint()
{
    if (!bIsSprinting)
    {
        return;
    }

    if (HasAuthority())
    {
        bIsSprinting = false;
        ApplyMovementSpeed();
    }
    else
    {
        ServerSetSprinting(false);
    }
}

void AGrandCityMobileCharacter::ServerSetSprinting_Implementation(bool bNewSprinting)
{
    bIsSprinting = bNewSprinting;
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
