#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "GrandCityMobileControlsWidget.generated.h"

class UTextBlock;
class UCanvasPanel;

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
 */
UCLASS()
class GRANDCITYMOBILE_API UGrandCityMobileControlsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    FSimpleDelegate OnRunPressed;
    FSimpleDelegate OnJumpPressed;
    FSimpleDelegate OnCrouchPressed;

    void ShowFeedback(const FText& FeedbackText);
    void ClearFeedback();

protected:
    virtual void NativeOnInitialized() override;

private:
    UButton* CreateActionButton(
        UCanvasPanel* Parent,
        FName ButtonName,
        const FText& Label,
        const FVector2D& Position);

    UFUNCTION()
    void HandleRunPressed();

    UFUNCTION()
    void HandleJumpPressed();

    UFUNCTION()
    void HandleCrouchPressed();

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> FeedbackLabel;

    UPROPERTY(Transient)
    TObjectPtr<UButton> RunButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> JumpButton;

    UPROPERTY(Transient)
    TObjectPtr<UButton> CrouchButton;
};
