#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GrandCityVehicle.generated.h"

class AGrandCityMobileCharacter;
class UBoxComponent;
class UMaterialInterface;
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
    void SetTouchBoost(bool bPressed) { bTouchBoostHeld = bPressed; }
    void ClearDriverInput();

    /** Server only. Installs the nitro boost upgrade (replicated to everyone). */
    void InstallNitroBoost();
    bool HasNitroBoost() const { return bHasNitroBoost; }

    /** Nitro tank as seen by the local driver: 0 empty, 1 full. */
    float GetNitroCharge() const { return NitroCharge; }
    bool IsNitroActive() const { return bNitroActive; }
    /** The tank ran dry and is refilling; boosting unlocks again at NitroReadyCharge. */
    bool IsNitroRecharging() const { return bNitroDepleted; }

    /** Server only. Repaints the body (replicated to everyone). */
    void SetPaintColor(const FLinearColor& NewColor);
    /** False while the vehicle still has its factory paint. */
    bool HasCustomPaint() const { return bHasCustomPaint; }
    const FLinearColor& GetPaintColor() const { return PaintColor; }

    /** Server only. Seats or removes the driver; possession is handled by the controller. */
    void SetDriver(AGrandCityMobileCharacter* NewDriver);
    AGrandCityMobileCharacter* GetDriver() const { return Driver; }
    bool HasDriver() const { return Driver != nullptr; }

    /** Distance from a world point to the vehicle's collision box surface (0 when inside). */
    float GetDistanceToVehicle(const FVector& WorldPoint) const;

    /** Height of the roof (top of the collision box) above the actor origin, in world cm. */
    float GetRoofHeight() const;

    /** Server only. Finds a free spot next to the vehicle for the driver to step out. */
    bool FindExitTransform(const AGrandCityMobileCharacter* Character, FVector& OutLocation, FRotator& OutRotation) const;

    float GetForwardSpeed() const { return ForwardSpeed; }

    /**
     * World transform of the seated driver's feet (animation root), facing the vehicle's
     * forward direction. The enter/exit animations are aligned to this point. By default
     * only the vehicle's yaw is kept; bFollowTilt also keeps its pitch and roll.
     */
    FTransform GetDriverSeatTransform(bool bFollowTilt = false) const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Interaction")
    float EnterRange = 220.0f;

    /**
     * Driver seat in vehicle space: X forward and Y right of the actor origin (scaled with
     * the actor), Z is height above the ground under the car. The seated animation keeps
     * the pelvis about 86 cm above this point.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Interaction")
    FVector DriverSeatOffset = FVector(10.0f, -45.0f, 10.0f);

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
    void KeyboardBoostPressed();
    void KeyboardBoostReleased();

    void FitCollisionToMesh();
    void SimulateMovement(float DeltaSeconds, float Throttle, float Steering, bool bBrake);
    void UpdateSpeed(float DeltaSeconds, float Throttle, bool bBrake);
    /** Drains the tank while boosting, refills it slowly otherwise, and sets bNitroActive. */
    void UpdateNitro(float DeltaSeconds, bool bWantsBoost);
    /** Local driver only. Widens the camera while boosting. */
    void UpdateNitroCamera(float DeltaSeconds);
    /** Highest forward speed the car can reach, with nitro when installed. */
    float GetTopSpeed() const;
    bool TraceGround(const FVector& WorldPoint, FVector& OutImpact, FVector& OutNormal) const;
    void MoveWithCollision(const FVector& Delta, const FQuat& NewRotation);
    void TickSimulatedProxy(float DeltaSeconds);

    /**
     * Pushes the paint into the body slots that use PaintableMaterial (dynamic instances are
     * created on first repaint). Every other material slot is left alone.
     */
    void ApplyPaint();

    UFUNCTION()
    void OnRep_Paint();

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

    /** Bought at a vehicle upgrade station; can also be ticked on a placed vehicle. */
    UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category="Vehicle|Nitro")
    bool bHasNitroBoost = false;

    /** Top speed while boosting, as a multiple of MaxForwardSpeed. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Nitro", meta=(ClampMin="1.0"))
    float NitroSpeedMultiplier = 1.5f;

    /** Acceleration while boosting (cm/s²), applied at full strength up to the boosted top speed. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Nitro", meta=(ClampMin="0.0"))
    float NitroAcceleration = 1600.0f;

    /** Seconds of boost a full tank lasts. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Nitro", meta=(ClampMin="0.1", Units="s"))
    float NitroDuration = 3.0f;

    /** Seconds to refill an empty tank. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Nitro", meta=(ClampMin="0.1", Units="s"))
    float NitroRechargeDuration = 12.0f;

    /** Pause after boosting before the tank starts to refill. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Nitro", meta=(ClampMin="0.0", Units="s"))
    float NitroRechargeDelay = 1.0f;

    /** After running dry, the tank has to refill to this fraction before it can boost again. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Nitro", meta=(ClampMin="0.0", ClampMax="1.0"))
    float NitroReadyCharge = 0.25f;

    /**
     * The body paint material. Only mesh slots using it (or an instance of it) are recoloured,
     * through its PaintColor / PaintTintAmount parameters.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Paint")
    TObjectPtr<UMaterialInterface> PaintableMaterial;

    /**
     * Off: factory paint from the texture. On: the paint area is recoloured with PaintColor.
     * Applied at runtime only, so a preset colour shows up once the game starts.
     */
    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Paint, BlueprintReadOnly, Category="Vehicle|Paint")
    bool bHasCustomPaint = false;

    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Paint, BlueprintReadOnly, Category="Vehicle|Paint",
        meta=(EditCondition="bHasCustomPaint", HideAlphaChannel))
    FLinearColor PaintColor = FLinearColor(0.6f, 0.02f, 0.02f);

    /** Extra camera field of view (degrees) while boosting. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle|Nitro", meta=(ClampMin="0.0", ClampMax="40.0"))
    float NitroCameraFOVBoost = 12.0f;

    float ForwardSpeed = 0.0f;
    float VerticalSpeed = 0.0f;
    float SmoothedSteering = 0.0f;
    float KeyboardThrottleInput = 0.0f;
    float KeyboardSteeringInput = 0.0f;
    float TouchThrottleInput = 0.0f;
    float TouchSteeringInput = 0.0f;
    bool bKeyboardBrakeHeld = false;
    bool bTouchBrakeHeld = false;
    bool bKeyboardBoostHeld = false;
    bool bTouchBoostHeld = false;
    bool bIsGrounded = false;
    float TimeSinceServerUpdate = 0.0f;

    float NitroCharge = 1.0f;
    float NitroRechargeDelayLeft = 0.0f;
    bool bNitroActive = false;
    bool bNitroDepleted = false;
    float BaseCameraFOV = 90.0f;

    bool bHasProxyTarget = false;
    FVector ProxyTargetLocation = FVector::ZeroVector;
    FRotator ProxyTargetRotation = FRotator::ZeroRotator;
};
