#include "UI/GrandCityQuestTrackerWidget.h"

#include "Quests/GrandCityQuestComponent.h"
#include "Quests/GrandCityQuestGiver.h"
#include "Quests/GrandCityQuestTarget.h"
#include "UI/GrandCityQuestOfferWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SafeZone.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace GrandCityQuestTracker
{
    constexpr float BannerDuration = 2.5f;
    constexpr float RefreshInterval = 0.1f;

    UTextBlock* MakeText(UWidgetTree* Tree, FName Name, int32 FontSize, const FLinearColor& Color, bool bWrap)
    {
        UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        Text->SetColorAndOpacity(FSlateColor(Color));
        Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
        Text->SetShadowOffset(FVector2D(1.5f, 1.5f));
        Text->SetAutoWrapText(bWrap);
        FSlateFontInfo Font = Text->GetFont();
        Font.Size = FontSize;
        Text->SetFont(Font);
        return Text;
    }
}

void UGrandCityQuestTrackerWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!WidgetTree || WidgetTree->RootWidget)
    {
        return;
    }

    // Purely informational: nothing here may swallow joystick or camera touches.
    SetVisibility(ESlateVisibility::HitTestInvisible);

    USafeZone* SafeZone = WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("QuestTrackerSafeZone"));
    WidgetTree->RootWidget = SafeZone;

    UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("QuestTrackerRoot"));
    SafeZone->AddChild(RootCanvas);

    TrackerPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("QuestTrackerPanel"));
    TrackerPanel->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
    TrackerPanel->SetPadding(FMargin(16.0f, 12.0f));
    TrackerPanel->SetVisibility(ESlateVisibility::Collapsed);
    if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(TrackerPanel))
    {
        PanelSlot->SetAnchors(FAnchors(0.0f, 0.0f));
        PanelSlot->SetAlignment(FVector2D::ZeroVector);
        PanelSlot->SetPosition(FVector2D(24.0f, 24.0f));
        PanelSlot->SetAutoSize(true);
    }

    USizeBox* TrackerSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("QuestTrackerSize"));
    TrackerSize->SetMaxDesiredWidth(460.0f);
    TrackerPanel->SetContent(TrackerSize);

    UVerticalBox* TrackerColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("QuestTrackerColumn"));
    TrackerSize->SetContent(TrackerColumn);

    TitleText = GrandCityQuestTracker::MakeText(WidgetTree, TEXT("QuestTrackerTitle"), 22, FLinearColor(1.0f, 0.82f, 0.15f), true);
    BodyText = GrandCityQuestTracker::MakeText(WidgetTree, TEXT("QuestTrackerBody"), 17, FLinearColor::White, true);
    if (UVerticalBoxSlot* TitleSlot = TrackerColumn->AddChildToVerticalBox(TitleText))
    {
        TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
    }
    TrackerColumn->AddChildToVerticalBox(BodyText);

    BannerBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("QuestBanner"));
    BannerBox->SetVisibility(ESlateVisibility::Collapsed);
    if (UCanvasPanelSlot* BannerSlot = RootCanvas->AddChildToCanvas(BannerBox))
    {
        BannerSlot->SetAnchors(FAnchors(0.5f, 0.22f));
        BannerSlot->SetAlignment(FVector2D(0.5f, 0.0f));
        BannerSlot->SetAutoSize(true);
    }

    BannerHeading = GrandCityQuestTracker::MakeText(WidgetTree, TEXT("QuestBannerHeading"), 40, FLinearColor::White, false);
    BannerHeading->SetJustification(ETextJustify::Center);
    BannerDetail = GrandCityQuestTracker::MakeText(WidgetTree, TEXT("QuestBannerDetail"), 22, FLinearColor::White, false);
    BannerDetail->SetJustification(ETextJustify::Center);
    if (UVerticalBoxSlot* HeadingSlot = BannerBox->AddChildToVerticalBox(BannerHeading))
    {
        HeadingSlot->SetHorizontalAlignment(HAlign_Center);
    }
    if (UVerticalBoxSlot* DetailSlot = BannerBox->AddChildToVerticalBox(BannerDetail))
    {
        DetailSlot->SetHorizontalAlignment(HAlign_Center);
    }
}

void UGrandCityQuestTrackerWidget::SetQuestComponent(UGrandCityQuestComponent* InQuestComponent)
{
    QuestComponent = InQuestComponent;
    RefreshTracker();
}

void UGrandCityQuestTrackerWidget::ShowBanner(const FText& Heading, const FText& Detail, const FLinearColor& Color)
{
    PendingBanners.Add({Heading, Detail, Color});
    if (BannerTimeLeft <= 0.0f)
    {
        ShowNextBanner();
    }
}

void UGrandCityQuestTrackerWidget::ShowNextBanner()
{
    if (!BannerBox)
    {
        return;
    }

    if (PendingBanners.IsEmpty())
    {
        BannerTimeLeft = 0.0f;
        BannerBox->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    const FBanner Banner = PendingBanners[0];
    PendingBanners.RemoveAt(0);
    BannerHeading->SetText(Banner.Heading);
    BannerHeading->SetColorAndOpacity(FSlateColor(Banner.Color));
    BannerDetail->SetText(Banner.Detail);
    BannerDetail->SetVisibility(Banner.Detail.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    BannerBox->SetVisibility(ESlateVisibility::HitTestInvisible);
    BannerTimeLeft = GrandCityQuestTracker::BannerDuration;
}

void UGrandCityQuestTrackerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (BannerTimeLeft > 0.0f)
    {
        BannerTimeLeft -= InDeltaTime;
        if (BannerTimeLeft <= 0.0f)
        {
            ShowNextBanner();
        }
    }

    RefreshTimeLeft -= InDeltaTime;
    if (RefreshTimeLeft <= 0.0f)
    {
        RefreshTimeLeft = GrandCityQuestTracker::RefreshInterval;
        RefreshTracker();
    }
}

void UGrandCityQuestTrackerWidget::RefreshTracker()
{
    const UGrandCityQuestComponent* Component = QuestComponent.Get();
    const AGrandCityQuestGiver* Quest = Component ? Component->GetActiveQuest() : nullptr;
    if (!TrackerPanel)
    {
        return;
    }
    if (!Quest)
    {
        TrackerPanel->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    const FGrandCityQuestDefinition& Definition = Quest->GetQuest();
    FString Title = Definition.Title.IsEmpty() ? TEXT("Quest") : Definition.Title.ToString();
    const float QuestTimeLeft = Component->GetRemainingQuestTime();
    if (QuestTimeLeft >= 0.0f)
    {
        Title += FString::Printf(TEXT("   %s"), *UGrandCityQuestOfferWidget::FormatTime(QuestTimeLeft));
    }
    TitleText->SetText(FText::FromString(Title));

    FString Body;
    const TArray<FGrandCityQuestObjectiveProgress>& Progress = Component->GetObjectiveProgress();
    for (int32 Index = 0; Index < Progress.Num() && Index < Definition.Objectives.Num(); ++Index)
    {
        const FGrandCityQuestObjective& Objective = Definition.Objectives[Index];
        const FGrandCityQuestObjectiveProgress& State = Progress[Index];
        if (State.Status == EGrandCityQuestObjectiveStatus::Locked)
        {
            continue;
        }

        FString Line = FString::Printf(TEXT("%s %s"),
            State.Status == EGrandCityQuestObjectiveStatus::Completed ? TEXT("[X]") : TEXT("[  ]"),
            *Objective.Description.ToString());

        if (State.Status == EGrandCityQuestObjectiveStatus::Active)
        {
            if (Objective.Type == EGrandCityQuestObjectiveType::Deliver)
            {
                TArray<AGrandCityQuestTarget*> Open;
                bool bPickup = false;
                Component->GetOpenDeliveryTargets(Index, Open, bPickup);
                const TCHAR* Verb = bPickup ? TEXT("Pick up at") : TEXT("Deliver to");
                if (Open.Num() == 1)
                {
                    Line += FString::Printf(TEXT("\n      > %s %s"), Verb, *Open[0]->GetDisplayName().ToString());
                }
                else if (Open.Num() > 1)
                {
                    Line += FString::Printf(TEXT("\n      > %s one of %d points"), Verb, Open.Num());
                }
            }
            else if (const AGrandCityQuestTarget* Next = Component->GetNextOrderedTarget(Index))
            {
                Line += FString::Printf(TEXT("\n      > Go to %s"), *Next->GetDisplayName().ToString());
            }
            if (State.RequiredCount > 1)
            {
                Line += FString::Printf(TEXT("  (%d/%d)"), State.Count, State.RequiredCount);
            }

            const float ObjectiveTimeLeft = Component->GetRemainingObjectiveTime(Index);
            if (ObjectiveTimeLeft >= 0.0f)
            {
                Line += FString::Printf(TEXT("  [%s]"), *UGrandCityQuestOfferWidget::FormatTime(ObjectiveTimeLeft));
            }
        }

        Body += (Body.IsEmpty() ? TEXT("") : TEXT("\n")) + Line;
    }

    BodyText->SetText(FText::FromString(Body));
    TrackerPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
}
