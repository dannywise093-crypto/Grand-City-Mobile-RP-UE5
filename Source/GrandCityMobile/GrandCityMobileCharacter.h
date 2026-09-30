#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Character.h"
#include "GrandCityMobileCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UAnimInstance;
class UAnimMontage;
class UAnimSequence;
class AGrandCityVehicle;

enum class EGrandCityCrouchAnimationPhase : uint8
{
    Standing,
    Entering,
    Crouched,
    Exiting
};

enum class EGrandCityVehicleTransition : uint8
{
    None,
    Entering,
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

    /** Server only. Seats the character in the driver seat (visible, riding along); the controller possesses the vehicle. */
    void EnterVehicle(AGrandCityVehicle* Vehicle);
    /** Server only. Restores the character at the given exit spot. */
    void ExitVehicle(const FVector& ExitLocation, const FRotator& ExitRotation);
    AGrandCityVehicle* GetOccupiedVehicle() const { return OccupiedVehicle; }

    /**
     * Server only. Moves the character to the driver door and plays the enter animation
     * on every machine. Returns the animation duration, or 0 when it cannot play (no free
     * spot at the door, crouched, missing asset) and the caller should seat it instantly.
     */
    float PlayEnterVehicleAnimation(AGrandCityVehicle* Vehicle);

    /**
     * Server only. Finds where the exit animation ends next to the driver door.
     * Returns false when that spot is blocked; the caller then uses an instant exit.
     */
    bool FindAnimatedVehicleExit(const AGrandCityVehicle* Vehicle, FVector& OutLocation, FRotator& OutRotation) const;

    /** Server only. Call right after ExitVehicle with the transform found above. */
    float PlayExitVehicleAnimation(const AGrandCityVehicle* Vehicle);

    /** Server only. Aborts an enter/exit animation on every machine. */
    void CancelVehicleAnimation();

    bool IsInVehicleTransition() const { return VehicleTransition != EGrandCityVehicleTransition::None; }

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

    UFUNCTION()
    void OnRep_OccupiedVehicle();

    void ApplyOccupiedVehicleState();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastPlayEnterVehicle(FVector_NetQuantize10 StartLocation, float StartYaw, float SeatHeight);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastPlayExitVehicle(FVector_NetQuantize10 SeatLocation, float SeatYaw);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastCancelVehicleAnimation();

    void BeginVehicleTransition(EGrandCityVehicleTransition Transition);
    void EndVehicleTransition(bool bStopMontage);
    void FinishExitVehicleAnimation();
    void TickVehicleTransition(float DeltaSeconds);
    float PlayVehicleMontage(UAnimSequence* Animation, bool bAutoBlendOut, float BlendInTime);
    float GetVehicleAnimationDuration(const UAnimSequence* Animation) const;
    /** 0..1 progress through Window (fractions of the clip) for the active vehicle montage. */
    float GetVehicleStepAlpha(const UAnimSequence* Animation, const FVector2D& Window) const;
    /** Places the mesh root at a world transform (the capsule stays where it is). */
    void SetMeshWorldPlacement(const FVector& RootLocation, const FQuat& RootRotation);
    void RestoreDefaultMeshPlacement();
    /** Holds the first (seated) frame of the exit clip while driving. */
    void PlaySeatedPose();
    void StopSeatedPose();
    /** Exit hand-off: runs right after each pose evaluation so the mesh placement matches that pose. */
    void HandleBoneTransformsFinalized();
    /** Floor + free-capsule + line-of-sight test for a spot next to the vehicle. */
    bool FindVehicleStandSpot(const AGrandCityVehicle* Vehicle, const FVector& DesiredFeetLocation, FVector& OutActorLocation) const;

    void ApplyMovementSpeed();
    void BeginCrouchLoop();
    void RestoreStandingAnimation();
    void UpdateCrouchLocomotion();
    void PlayCrouchAnimation(UAnimSequence* Animation, bool bLooping, float PlayRate = 1.0f);
    UAnimSequence* SelectCrouchLocomotionAnimation() const;
    bool IsMovingForCrouchTransition() const;
    void PauseMovementForCrouchTransition(float DurationSeconds);
    void ResumeMovementAfterCrouchTransition();
#if !UE_BUILD_SHIPPING
    void TickCrouchMovementPlaytest();
#endif

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

    UPROPERTY(ReplicatedUsing=OnRep_OccupiedVehicle, BlueprintReadOnly, Category="Vehicle")
    TObjectPtr<AGrandCityVehicle> OccupiedVehicle;

    UPROPERTY(EditAnywhere, Category="Vehicle|Animation")
    TObjectPtr<UAnimSequence> EnterVehicleAnimation;

    UPROPERTY(EditAnywhere, Category="Vehicle|Animation")
    TObjectPtr<UAnimSequence> ExitVehicleAnimation;

    /** Both clips are long (5-6 s); a higher rate keeps getting in and out snappy. */
    UPROPERTY(EditAnywhere, Category="Vehicle|Animation", meta=(ClampMin="0.1"))
    float VehicleAnimationPlayRate = 1.4f;

    /**
     * Where the pelvis ends in the enter clip relative to where it starts, in the
     * character's starting frame (X forward, Y right). Measured from MM_Car_Enter.
     */
    UPROPERTY(EditAnywhere, Category="Vehicle|Animation")
    FVector EnterAnimationSeatOffset = FVector(178.0f, 12.4f, 0.0f);

    /** Facing at the start of the enter clip, relative to the vehicle's forward (90 = facing the car's side). */
    UPROPERTY(EditAnywhere, Category="Vehicle|Animation")
    float EnterAnimationStartYaw = 90.0f;

    /**
     * Where the character stands at the end of the exit clip relative to the seat, in the
     * vehicle's frame (X forward, Y right). Measured from MM_Car_Exit.
     */
    UPROPERTY(EditAnywhere, Category="Vehicle|Animation")
    FVector ExitAnimationEndOffset = FVector(19.4f, -182.6f, 0.0f);

    /**
     * Part of the enter clip (fractions of its length) during which the body is raised
     * from the ground to the seat height; roughly while the character climbs in.
     */
    UPROPERTY(EditAnywhere, Category="Vehicle|Animation")
    FVector2D EnterAnimationStepWindow = FVector2D(0.42f, 0.62f);

    /** Part of the exit clip during which the body is lowered from the seat to the ground. */
    UPROPERTY(EditAnywhere, Category="Vehicle|Animation")
    FVector2D ExitAnimationStepWindow = FVector2D(0.30f, 0.45f);

    /** Facing at the end of the exit clip, relative to the vehicle's forward (-90 = facing away, left). */
    UPROPERTY(EditAnywhere, Category="Vehicle|Animation")
    float ExitAnimationEndYaw = -90.0f;

    /** Time to slide from where the player stood to the start of the enter clip. */
    UPROPERTY(EditAnywhere, Category="Vehicle|Animation", meta=(ClampMin="0.0"))
    float VehicleAlignDuration = 0.3f;

    UPROPERTY(Transient)
    TObjectPtr<UAnimMontage> ActiveVehicleMontage;

    UPROPERTY(Transient)
    TObjectPtr<UAnimMontage> SeatedMontage;

    /** World Z of the seat (animation root) for the enter clip's climb-in. */
    float VehicleSeatHeight = 0.0f;

    FDelegateHandle BoneTransformsFinalizedHandle;
    /** Where the pelvis was when the exit clip started blending out. */
    FVector ExitHandOffPelvisAnchor = FVector::ZeroVector;
    bool bExitHandOffAnchored = false;

    EGrandCityVehicleTransition VehicleTransition = EGrandCityVehicleTransition::None;
    FTimerHandle VehicleTransitionTimer;
    bool bVehicleAligning = false;
    float VehicleAlignElapsed = 0.0f;
    FVector VehicleAlignFromLocation = FVector::ZeroVector;
    FQuat VehicleAlignFromRotation = FQuat::Identity;
    FVector VehicleAlignToLocation = FVector::ZeroVector;
    FQuat VehicleAlignToRotation = FQuat::Identity;
    /** Exit clip: the mesh root stays pinned to the seat while the capsule already waits outside. */
    FVector ExitSeatLocation = FVector::ZeroVector;
    float ExitSeatYaw = 0.0f;
    FVector DefaultMeshRelativeLocation = FVector::ZeroVector;
    FRotator DefaultMeshRelativeRotation = FRotator::ZeroRotator;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> ActiveCrouchAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimMontage> ActiveCrouchMontage;

    FTimerHandle CrouchAnimationTimer;
    FTimerHandle CrouchMovementPauseTimer;
    EGrandCityCrouchAnimationPhase CrouchAnimationPhase = EGrandCityCrouchAnimationPhase::Standing;
    bool bCrouchMovementPaused = false;
    float ForwardInputValue = 0.0f;
    float RightInputValue = 0.0f;

#if !UE_BUILD_SHIPPING
    bool bCrouchMovementPlaytestEnabled = false;
    bool bCrouchMovementPlaytestFailed = false;
    int32 CrouchMovementPlaytestStage = 0;
    int32 CrouchMovementPlaytestPauseCount = 0;
    int32 CrouchMovementPlaytestCompletedPauses = 0;
    float CrouchMovementPlaytestStageStartTime = -1.0f;
    FVector CrouchMovementPlaytestStageStartLocation = FVector::ZeroVector;
    FVector CrouchMovementPlaytestPauseStartLocation = FVector::ZeroVector;
    float CrouchMovementPlaytestMaxPauseDrift = 0.0f;
#endif
};
