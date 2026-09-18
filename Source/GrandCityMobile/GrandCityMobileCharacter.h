#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Character.h"
#include "GrandCityMobileCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UAnimInstance;
class UAnimSequence;

enum class EGrandCityCrouchAnimationPhase : uint8
{
    Standing,
    Entering,
    Crouched,
    Exiting
};

UCLASS()
class GRANDCITYMOBILE_API AGrandCityMobileCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AGrandCityMobileCharacter();

    /** Gameplay actions shared by keyboard, gamepad, and the mobile action buttons. */
    void StartSprint();
    void StopSprint();
    void ToggleSprint();
    void ToggleCrouch();
    bool IsSprinting() const { return bIsSprinting; }
    bool WantsToCrouch() const;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
    virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);

    UFUNCTION(Server, Reliable)
    void ServerSetSprinting(bool bNewSprinting);

    UFUNCTION()
    void OnRep_Sprinting();

    void ApplyMovementSpeed();
    void BeginCrouchLoop();
    void RestoreStandingAnimation();
    void UpdateCrouchLocomotion();
    void PlayCrouchAnimation(UAnimSequence* Animation, bool bLooping, float PlayRate = 1.0f);
    UAnimSequence* SelectCrouchLocomotionAnimation() const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(Transient)
    TSubclassOf<UAnimInstance> StandingAnimInstanceClass;

    UPROPERTY()
    TObjectPtr<UAnimSequence> CrouchEntryAnimation;

    UPROPERTY()
    TObjectPtr<UAnimSequence> CrouchExitAnimation;

    UPROPERTY()
    TObjectPtr<UAnimSequence> CrouchIdleAnimation;

    UPROPERTY()
    TObjectPtr<UAnimSequence> CrouchWalkForwardAnimation;

    UPROPERTY()
    TObjectPtr<UAnimSequence> CrouchWalkBackwardAnimation;

    UPROPERTY()
    TObjectPtr<UAnimSequence> CrouchWalkLeftAnimation;

    UPROPERTY()
    TObjectPtr<UAnimSequence> CrouchWalkRightAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement")
    float WalkSpeed = 450.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement")
    float SprintSpeed = 650.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement")
    float CrouchSpeed = 250.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Sprinting, BlueprintReadOnly, Category="Movement")
    bool bIsSprinting = false;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> ActiveCrouchAnimation;

    FTimerHandle CrouchAnimationTimer;
    EGrandCityCrouchAnimationPhase CrouchAnimationPhase = EGrandCityCrouchAnimationPhase::Standing;
};
