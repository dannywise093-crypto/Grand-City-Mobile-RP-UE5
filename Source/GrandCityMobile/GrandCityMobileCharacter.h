#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GrandCityMobileCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;

UCLASS()
class GRANDCITYMOBILE_API AGrandCityMobileCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AGrandCityMobileCharacter();

protected:
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);
    void StartSprint();
    void StopSprint();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement")
    float WalkSpeed = 450.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement")
    float SprintSpeed = 650.0f;
};
