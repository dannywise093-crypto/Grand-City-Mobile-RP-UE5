#include "UI/GrandCityVehicleHudWidget.h"

#include "Vehicles/GrandCityVehicle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SafeZone.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace GrandCityVehicleHud
{
    constexpr float MessageDuration = 3.0f;
    const FLinearColor NitroReadyColor(0.1f, 0.75f, 1.0f);
    const FLinearColor NitroActiveColor(0.55f, 0.95f, 1.0f);
    const FLinearColor NitroRechargingColor(1.0f, 0.45f, 0.1f);

    UTextBlock* MakeText(UWidgetTree* Tree, FName Name, int32 FontSize, const FLinearColor& Color)
    {
        UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        Text->SetColorAndOpacity(FSlateColor(Color));
        Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
        Text->SetShadowOffset(FVector2D(1.5f, 1.5f));
        Text->SetJustification(ETextJustify::Center);
        FSlateFontInfo Font = Text->GetFont();
        Font.Size = FontSize;
        Text->SetFont(Font);
        return Text;
    }
}

void UGrandCityVehicleHudWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!WidgetTree || WidgetTree->RootWidget)
    {
        return;
    }

    // Purely informational: nothing here may swallow joystick, pedal or camera touches.
    SetVisibility(ESlateVisibility::HitTestInvisible);

    USafeZone* SafeZone = WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("VehicleHudSafeZone"));
    WidgetTree->RootWidget = SafeZone;

    UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("VehicleHudRoot"));
    SafeZone->AddChild(RootCanvas);

    // Nitro gauge, bottom centre between the steering and pedal buttons.
    NitroPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NitroPanel"));
    NitroPanel->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
    NitroPanel->SetPadding(FMargin(14.0f, 8.0f));
    NitroPanel->SetVisibility(ESlateVisibility::Collapsed);
    if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(NitroPanel))
    {
        PanelSlot->SetAnchors(FAnchors(0.5f, 1.0f));
        PanelSlot->SetAlignment(FVector2D(0.5f, 1.0f));
        PanelSlot->SetPosition(FVector2D(0.0f, -24.0f));
        PanelSlot->SetAutoSize(true);
    }

    UVerticalBox* NitroColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NitroColumn"));
    NitroPanel->SetContent(NitroColumn);

    NitroLabel = GrandCityVehicleHud::MakeText(WidgetTree, TEXT("NitroLabel"), 18, FLinearColor::White);
    if (UVerticalBoxSlot* LabelSlot = NitroColumn->AddChildToVerticalBox(NitroLabel))
    {
        LabelSlot->SetHorizontalAlignment(HAlign_Center);
        LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
    }

    USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("NitroBarSize"));
    BarSize->SetWidthOverride(320.0f);
    BarSize->SetHeightOverride(16.0f);
    NitroColumn->AddChildToVerticalBox(BarSize);

    NitroBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("NitroBar"));
    NitroBar->SetFillColorAndOpacity(GrandCityVehicleHud::NitroReadyColor);
    NitroBar->SetPercent(1.0f);
    BarSize->SetContent(NitroBar);

    PromptText = GrandCityVehicleHud::MakeText(WidgetTree, TEXT("UpgradePrompt"), 22, FLinearColor(0.6f, 1.0f, 0.65f));
    PromptText->SetVisibility(ESlateVisibility::Collapsed);
    if (UCanvasPanelSlot* PromptSlot = RootCanvas->AddChildToCanvas(PromptText))
    {
        PromptSlot->SetAnchors(FAnchors(0.5f, 0.66f));
        PromptSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        PromptSlot->SetAutoSize(true);
    }

    MessagePanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("UpgradeMessage"));
    MessagePanel->SetVisibility(ESlateVisibility::Collapsed);
    if (UCanvasPanelSlot* MessageSlot = RootCanvas->AddChildToCanvas(MessagePanel))
    {
        MessageSlot->SetAnchors(FAnchors(0.5f, 0.3f));
        MessageSlot->SetAlignment(FVector2D(0.5f, 0.0f));
        MessageSlot->SetAutoSize(true);
    }

    MessageHeading = GrandCityVehicleHud::MakeText(WidgetTree, TEXT("UpgradeMessageHeading"), 38, FLinearColor(0.3f, 1.0f, 0.4f));
    MessageDetail = GrandCityVehicleHud::MakeText(WidgetTree, TEXT("UpgradeMessageDetail"), 22, FLinearColor::White);
    if (UVerticalBoxSlot* HeadingSlot = MessagePanel->AddChildToVerticalBox(MessageHeading))
    {
        HeadingSlot->SetHorizontalAlignment(HAlign_Center);
    }
    if (UVerticalBoxSlot* DetailSlot = MessagePanel->AddChildToVerticalBox(MessageDetail))
    {
        DetailSlot->SetHorizontalAlignment(HAlign_Center);
    }
}

void UGrandCityVehicleHudWidget::SetPrompt(const FText& Prompt)
{
    if (!PromptText)
    {
        return;
    }

    if (Prompt.IsEmpty())
    {
        PromptText->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    if (!PromptText->GetText().EqualTo(Prompt))
    {
        PromptText->SetText(Prompt);
    }
    PromptText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UGrandCityVehicleHudWidget::ShowMessage(const FText& Heading, const FText& Detail)
{
    if (!MessagePanel)
    {
        return;
    }

    MessageHeading->SetText(Heading);
    MessageDetail->SetText(Detail);
    MessageDetail->SetVisibility(Detail.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    MessagePanel->SetVisibility(ESlateVisibility::HitTestInvisible);
    MessageTimeLeft = GrandCityVehicleHud::MessageDuration;
}

void UGrandCityVehicleHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (MessageTimeLeft > 0.0f)
    {
        MessageTimeLeft -= InDeltaTime;
        if (MessageTimeLeft <= 0.0f && MessagePanel)
        {
            MessagePanel->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    RefreshNitroGauge();
}

void UGrandCityVehicleHudWidget::RefreshNitroGauge()
{
    if (!NitroPanel)
    {
        return;
    }

    const AGrandCityVehicle* Vehicle = Cast<AGrandCityVehicle>(GetOwningPlayerPawn());
    if (!Vehicle || !Vehicle->HasNitroBoost())
    {
        NitroPanel->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    FText Label;
    FLinearColor Color;
    if (Vehicle->IsNitroActive())
    {
        Label = NSLOCTEXT("GrandCityVehicleHud", "NitroActive", "NITRO!");
        Color = GrandCityVehicleHud::NitroActiveColor;
    }
    else if (Vehicle->IsNitroRecharging())
    {
        Label = NSLOCTEXT("GrandCityVehicleHud", "NitroRecharging", "NITRO RECHARGING");
        Color = GrandCityVehicleHud::NitroRechargingColor;
    }
    else
    {
        Label = PLATFORM_DESKTOP
            ? NSLOCTEXT("GrandCityVehicleHud", "NitroReadyKey", "NITRO  [SHIFT]")
            : NSLOCTEXT("GrandCityVehicleHud", "NitroReady", "NITRO");
        Color = GrandCityVehicleHud::NitroReadyColor;
    }

    if (!NitroLabel->GetText().EqualTo(Label))
    {
        NitroLabel->SetText(Label);
    }
    NitroBar->SetFillColorAndOpacity(Color);
    NitroBar->SetPercent(Vehicle->GetNitroCharge());
    NitroPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
}
