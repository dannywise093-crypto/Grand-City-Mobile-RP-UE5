#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "GrandCityMobileControlsWidget.generated.h"

class UTextBlock;
class UCanvasPanel;

enum class EGrandCityVehicleControl : uint8
{
    Forward,
    Brake,
    Reverse,
    SteerLeft,
    SteerRight
};

DECLARE_DELEGATE_TwoParams(FGrandCityVehicleControlDelegate, EGrandCityVehicleControl, bool /* bPressed */);

/** Button variant initialized without keyboard focus before its Slate widget exists. */
UCLASS()
class GRANDCITYMOBILE_API UGrandCityMobileActionButton : public UButton
{
    GENERATED_BODY()

public:
    explicit UGrandCityMobileActionButton(const FObjectInitializer& ObjectInitializer);
};

/**
 * Mobile action buttons plus text feedback for the left joystick and camera swipe.
 * Empty space remains hit-test transparent so viewport touches still rotate the camera.
 * While driving, the on-foot buttons are swapped for vehicle pedals and steering.
 */
UCLASS()
class GRANDCITYMOBILE_API UGrandCityMobileControlsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    FSimpleDelegate OnRunPressed;
    FSimpleDelegate OnJumpPressed;
    FSimpleDelegate OnCrouchPressed;
    FSimpleDelegate OnEnterVehiclePressed;
    FSimpleDelegate OnExitVehiclePressed;
    FGrandCityVehicleControlDelegate OnVehicleControlChanged;

    void ShowFeedback(const FText& FeedbackText);
    void ClearFeedback();

    /** Swaps the on-foot buttons for the driving buttons. */
    void SetVehicleMode(bool bInVehicle);
    /** Shows the ENTER button while the player stands next to a free vehicle. */
    void SetEnterVehicleAvailable(bool bAvailable);

protected:
    virtual void NativeOnInitialized() override;

private:
    UButton* CreateActionButton(
        UCanvasPanel* Parent,
        FName ButtonName,
        const FText& Label,
        const FVector2D& Position,
        const FVector2D& Anchor = FVector2D(1.0f, 1.0f),
        const FVector2D& Size = FVector2D(160.0f, 112.0f),
        bool bHoldButton = false);

    void RefreshButtonVisibility();

    UFUNCTION()
    void HandleRunPressed();

    UFUNCTION()
    void HandleJumpPressed();

    UFUNCTION()
    void HandleCrouchPressed();

    UFUNCTION()
    void HandleEnterVehiclePressed();

    UFUNCTION()
    void HandleExitVehiclePressed();

    UFUNCTION()
    void HandleForwardPressed();

    UFUNCTION()
    void HandleForwardReleased();

    UFUNCTION()
    void HandleBrakePressed();

    UFUNCTION()
    void HandleBrakeReleased();

    UFUNCTION()
    void HandleReversePressed();

    UFUNCTION()
    void HandleReverseReleased();

    UFUNCTION()
    void HandleSteerLeftPressed();

    UFUNCTION()
    void HandleSteerLeftReleased();

    UFUNCTION()
    void HandleSteerRightPressed();

    UFUNCTION()
    void HandleSteerRightReleased();

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> FeedbackLabel;

    UPROPERTY(Transient)
    TObjectPtr<UButton> RunButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> JumpButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> CrouchButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> EnterVehicleButton;

    /** On-foot buttons (RUN, JUMP, CROUCH). */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UButton>> CharacterButtons;

    /** Driving buttons (FORWARD, BRAKE, REVERSE, EXIT, LEFT, RIGHT). */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UButton>> VehicleButtons;

    bool bVehicleMode = false;
    bool bEnterVehicleAvailable = false;
};
