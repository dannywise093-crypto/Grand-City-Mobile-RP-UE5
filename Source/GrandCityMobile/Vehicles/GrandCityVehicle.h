#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GrandCityVehicle.generated.h"

class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;

UCLASS()
class GRANDCITYMOBILE_API AGrandCityVehicle : public APawn
{
    GENERATED_BODY()

public:
    AGrandCityVehicle();

protected:
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void Tick(float DeltaSeconds) override;

    void Throttle(float Value);
    void Steer(float Value);
    void Brake();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle")
    TObjectPtr<UStaticMeshComponent> VehicleBody;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Camera")
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Handling")
    float Acceleration = 900000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Handling")
    float MaxSpeed = 6000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Handling")
    float SteeringTorque = 1800000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Handling")
    float BrakeStrength = 1200000.0f;

    float ThrottleInput = 0.0f;
    float SteeringInput = 0.0f;
};
