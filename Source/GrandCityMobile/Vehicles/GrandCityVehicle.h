#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GrandCityVehicle.generated.h"

class AGrandCityMobileCharacter;
class UBoxComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;

/**
 * Arcade-style drivable vehicle. Movement is kinematic (swept box + ground traces)
 * instead of physics so it stays predictable on mobile hardware. The driving client
 * simulates locally and streams its transform to the server, which replicates it to
 * everyone else.
 */
UCLASS()
class GRANDCITYMOBILE_API AGrandCityVehicle : public APawn
{
    GENERATED_BODY()

public:
    AGrandCityVehicle();

    /** Touch buttons feed these; keyboard axes are combined with them each tick. */
    void SetTouchThrottle(float Value) { TouchThrottleInput = FMath::Clamp(Value, -1.0f, 1.0f); }
    void SetTouchSteering(float Value) { TouchSteeringInput = FMath::Clamp(Value, -1.0f, 1.0f); }
    void SetTouchBrake(bool bPressed) { bTouchBrakeHeld = bPressed; }
    void ClearDriverInput();

    /** Server only. Seats or removes the driver; possession is handled by the controller. */
    void SetDriver(AGrandCityMobileCharacter* NewDriver);
    AGrandCityMobileCharacter* GetDriver() const { return Driver; }
    bool HasDriver() const { return Driver != nullptr; }

    /** Distance from a world point to the vehicle's collision box surface (0 when inside). */
    float GetDistanceToVehicle(const FVector& WorldPoint) const;

    /** Server only. Finds a free spot next to the vehicle for the driver to step out. */
    bool FindExitTransform(const AGrandCityMobileCharacter* Character, FVector& OutLocation, FRotator& OutRotation) const;

    float GetForwardSpeed() const { return ForwardSpeed; }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Interaction")
    float EnterRange = 220.0f;

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PostNetReceiveLocationAndRotation() override;
    virtual void UnPossessed() override;

    void KeyboardThrottle(float Value);
    void KeyboardSteer(float Value);
    void KeyboardBrakePressed();
    void KeyboardBrakeReleased();

    void FitCollisionToMesh();
    void SimulateMovement(float DeltaSeconds, float Throttle, float Steering, bool bBrake);
    void UpdateSpeed(float DeltaSeconds, float Throttle, bool bBrake);
    bool TraceGround(const FVector& WorldPoint, FVector& OutImpact, FVector& OutNormal) const;
    void MoveWithCollision(const FVector& Delta, const FQuat& NewRotation);
    void TickSimulatedProxy(float DeltaSeconds);

    UFUNCTION(Server, Unreliable)
    void ServerUpdateMovement(FVector_NetQuantize10 NewLocation, FRotator NewRotation, float NewForwardSpeed);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle")
    TObjectPtr<UBoxComponent> CollisionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle")
    TObjectPtr<UStaticMeshComponent> VehicleBody;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Camera")
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Vehicle")
    TObjectPtr<AGrandCityMobileCharacter> Driver;

    /** Rotates the mesh if an imported model does not face +X. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Setup")
    float MeshYawOffset = 0.0f;

    /**
     * Gap (world cm, independent of actor scale) between the ground and the bottom of the
     * collision box. Curbs lower than this pass under the box and the car climbs them;
     * taller ones block it like a wall.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Setup", meta=(ClampMin="0.0", Units="cm"))
    float GroundClearance = 25.0f;

    /** Raises (positive) or lowers (negative) the whole car above the ground, in world cm. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Setup", meta=(Units="cm"))
    float RideHeightOffset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Handling")
    float MaxForwardSpeed = 2200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Handling")
    float MaxReverseSpeed = 700.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Handling")
    float Acceleration = 900.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Handling")
    float ReverseAcceleration = 600.0f;

    /** Deceleration while braking (brake button, or throttle against the direction of travel). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Handling")
    float BrakeDeceleration = 2600.0f;

    /** Deceleration when no pedal is held. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Handling")
    float CoastDeceleration = 450.0f;

    /** Maximum yaw rate in degrees per second. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Handling")
    float MaxSteeringRate = 75.0f;

    /** Speed at which steering reaches full authority; slower cars turn proportionally less. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Handling")
    float FullSteeringSpeed = 500.0f;

    /** Fraction of steering kept at top speed, so high-speed turns stay stable. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Handling", meta=(ClampMin="0.1", ClampMax="1.0"))
    float HighSpeedSteeringFactor = 0.55f;

    /** How fast the steering input ramps toward the pressed direction (per second). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Handling")
    float SteeringResponse = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Handling")
    float Gravity = 1960.0f;

    float ForwardSpeed = 0.0f;
    float VerticalSpeed = 0.0f;
    float SmoothedSteering = 0.0f;
    float KeyboardThrottleInput = 0.0f;
    float KeyboardSteeringInput = 0.0f;
    float TouchThrottleInput = 0.0f;
    float TouchSteeringInput = 0.0f;
    bool bKeyboardBrakeHeld = false;
    bool bTouchBrakeHeld = false;
    bool bIsGrounded = false;
    float TimeSinceServerUpdate = 0.0f;

    bool bHasProxyTarget = false;
    FVector ProxyTargetLocation = FVector::ZeroVector;
    FRotator ProxyTargetRotation = FRotator::ZeroRotator;
};
