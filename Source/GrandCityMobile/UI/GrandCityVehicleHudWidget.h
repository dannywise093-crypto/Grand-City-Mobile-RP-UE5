#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GrandCityVehicleHudWidget.generated.h"

class UBorder;
class UProgressBar;
class UTextBlock;
class UVerticalBox;

/**
 * Driving HUD: the nitro gauge (while driving a car that has nitro), the upgrade station
 * prompt, and a short message when an upgrade is installed. Purely informational, so it
 * never swallows touches.
 */
UCLASS()
class GRANDCITYMOBILE_API UGrandCityVehicleHudWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Empty text hides the prompt. */
    void SetPrompt(const FText& Prompt);
    void ShowMessage(const FText& Heading, const FText& Detail);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void RefreshNitroGauge();

    UPROPERTY(Transient)
    TObjectPtr<UBorder> NitroPanel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> NitroLabel;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> NitroBar;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> PromptText;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> MessagePanel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> MessageHeading;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> MessageDetail;

    float MessageTimeLeft = 0.0f;
};
