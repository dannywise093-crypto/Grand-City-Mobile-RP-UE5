#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GrandCityQuestTrackerWidget.generated.h"

class UBorder;
class UTextBlock;
class UVerticalBox;
class UGrandCityQuestComponent;

/** HUD showing the active quest's objectives, progress and timers, plus result banners. Never takes input. */
UCLASS()
class GRANDCITYMOBILE_API UGrandCityQuestTrackerWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetQuestComponent(UGrandCityQuestComponent* InQuestComponent);

    /** Queues a short centre-screen message ("QUEST COMPLETE", "QUEST FAILED"...). */
    void ShowBanner(const FText& Heading, const FText& Detail, const FLinearColor& Color);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    struct FBanner
    {
        FText Heading;
        FText Detail;
        FLinearColor Color;
    };

    void RefreshTracker();
    void ShowNextBanner();

    TWeakObjectPtr<UGrandCityQuestComponent> QuestComponent;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> TrackerPanel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> BodyText;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> BannerBox;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> BannerHeading;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> BannerDetail;

    TArray<FBanner> PendingBanners;
    float BannerTimeLeft = 0.0f;
    float RefreshTimeLeft = 0.0f;
};
