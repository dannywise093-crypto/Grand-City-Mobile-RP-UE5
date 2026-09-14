#include "GrandCityVehicle.h"

#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"

AGrandCityVehicle::AGrandCityVehicle()
{
    PrimaryActorTick.bCanEverTick = true;

    VehicleBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VehicleBody"));
    SetRootComponent(VehicleBody);
    VehicleBody->SetSimulatePhysics(true);
    VehicleBody->SetEnableGravity(true);
    VehicleBody->SetLinearDamping(0.35f);
    VehicleBody->SetAngularDamping(2.0f);
    VehicleBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> VehicleMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (VehicleMesh.Succeeded())
    {
        VehicleBody->SetStaticMesh(VehicleMesh.Object);
        VehicleBody->SetRelativeScale3D(FVector(2.2f, 1.1f, 0.55f));
    }

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(VehicleBody);
    CameraBoom->TargetArmLength = 650.0f;
    CameraBoom->SetRelativeRotation(FRotator(-12.0f, 0.0f, 0.0f));
    CameraBoom->bDoCollisionTest = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
}

void AGrandCityVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAxis(TEXT("VehicleThrottle"), this, &AGrandCityVehicle::Throttle);
    PlayerInputComponent->BindAxis(TEXT("VehicleSteer"), this, &AGrandCityVehicle::Steer);
    PlayerInputComponent->BindAction(TEXT("VehicleBrake"), IE_Pressed, this, &AGrandCityVehicle::Brake);
}

void AGrandCityVehicle::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!VehicleBody || !VehicleBody->IsSimulatingPhysics())
    {
        return;
    }

    const FVector Forward = GetActorForwardVector();
    const FVector Velocity = VehicleBody->GetPhysicsLinearVelocity();
    const float ForwardSpeed = FVector::DotProduct(Velocity, Forward);

    if (FMath::Abs(ForwardSpeed) < MaxSpeed || FMath::Sign(ThrottleInput) != FMath::Sign(ForwardSpeed))
    {
        VehicleBody->AddForce(Forward * ThrottleInput * Acceleration);
    }

    if (FMath::Abs(SteeringInput) > KINDA_SMALL_NUMBER && FMath::Abs(ForwardSpeed) > 100.0f)
    {
        const float SteeringDirection = FMath::Sign(ForwardSpeed);
        VehicleBody->AddTorqueInRadians(FVector(0.0f, 0.0f, SteeringInput * SteeringTorque * SteeringDirection));
    }
}

void AGrandCityVehicle::Throttle(float Value)
{
    ThrottleInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void AGrandCityVehicle::Steer(float Value)
{
    SteeringInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void AGrandCityVehicle::Brake()
{
    if (VehicleBody && VehicleBody->IsSimulatingPhysics())
    {
        const FVector Velocity = VehicleBody->GetPhysicsLinearVelocity();
        VehicleBody->AddForce(-Velocity.GetSafeNormal() * BrakeStrength);
    }
}
