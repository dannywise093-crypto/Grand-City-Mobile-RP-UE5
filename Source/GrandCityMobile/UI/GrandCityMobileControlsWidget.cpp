#include "UI/GrandCityMobileControlsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SafeZone.h"
#include "Components/TextBlock.h"
#include "Widgets/Layout/Anchors.h"

UGrandCityMobileActionButton::UGrandCityMobileActionButton(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    InitIsFocusable(false);
}

void UGrandCityMobileControlsWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!WidgetTree || WidgetTree->RootWidget)
    {
        return;
    }

    USafeZone* SafeZone = WidgetTree->ConstructWidget<USafeZone>(
        USafeZone::StaticClass(), TEXT("MobileControlsSafeZone"));
    SafeZone->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    SafeZone->SetSidesToPad(true, true, true, true);
    WidgetTree->RootWidget = SafeZone;

    UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), TEXT("MobileControlsRoot"));
    RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    SafeZone->AddChild(RootCanvas);

    FeedbackLabel = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), TEXT("TouchFeedbackLabel"));
    FeedbackLabel->SetJustification(ETextJustify::Center);
    FeedbackLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    FeedbackLabel->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
    FeedbackLabel->SetShadowOffset(FVector2D(2.0f, 2.0f));
    FeedbackLabel->SetVisibility(ESlateVisibility::Collapsed);

    FSlateFontInfo FeedbackFont = FeedbackLabel->GetFont();
    FeedbackFont.Size = 28;
    FeedbackLabel->SetFont(FeedbackFont);

    if (UCanvasPanelSlot* FeedbackSlot = RootCanvas->AddChildToCanvas(FeedbackLabel))
    {
        FeedbackSlot->SetAnchors(FAnchors(0.5f, 0.08f));
        FeedbackSlot->SetAlignment(FVector2D(0.5f, 0.0f));
        FeedbackSlot->SetPosition(FVector2D::ZeroVector);
        FeedbackSlot->SetSize(FVector2D(520.0f, 64.0f));
    }

    // Anchor the cluster to the bottom-right of the safe area. The generous
    // button sizes remain usable after Unreal's mobile DPI scaling is applied.
    RunButton = CreateActionButton(
        RootCanvas,
        TEXT("RunButton"),
        NSLOCTEXT("GrandCityMobileControls", "RunButton", "RUN"),
        FVector2D(-208.0f, -32.0f));
    JumpButton = CreateActionButton(
        RootCanvas,
        TEXT("JumpButton"),
        NSLOCTEXT("GrandCityMobileControls", "JumpButton", "JUMP"),
        FVector2D(-32.0f, -32.0f));
    CrouchButton = CreateActionButton(
        RootCanvas,
        TEXT("CrouchButton"),
        NSLOCTEXT("GrandCityMobileControls", "CrouchButton", "CROUCH"),
        FVector2D(-32.0f, -160.0f));

    if (RunButton)
    {
        RunButton->OnClicked.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleRunPressed);
    }
    if (JumpButton)
    {
        JumpButton->OnClicked.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleJumpPressed);
    }
    if (CrouchButton)
    {
        CrouchButton->OnClicked.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleCrouchPressed);
    }
}

UButton* UGrandCityMobileControlsWidget::CreateActionButton(
    UCanvasPanel* Parent,
    FName ButtonName,
    const FText& Label,
    const FVector2D& Position)
{
    if (!WidgetTree || !Parent)
    {
        return nullptr;
    }

    UButton* ActionButton = WidgetTree->ConstructWidget<UGrandCityMobileActionButton>(
        UGrandCityMobileActionButton::StaticClass(), ButtonName);
    ActionButton->SetVisibility(ESlateVisibility::Visible);
    ActionButton->SetBackgroundColor(FLinearColor(0.04f, 0.06f, 0.09f, 0.82f));
    ActionButton->SetColorAndOpacity(FLinearColor::White);
    // Mobile actions fire as soon as their own finger goes down. This mode does
    // not capture the pointer, so other fingers remain available to the virtual
    // joystick, camera swipe, and the other action buttons.
    ActionButton->SetTouchMethod(EButtonTouchMethod::Down);

    UTextBlock* ButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), *FString::Printf(TEXT("%sLabel"), *ButtonName.ToString()));
    ButtonLabel->SetText(Label);
    ButtonLabel->SetJustification(ETextJustify::Center);
    ButtonLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    ButtonLabel->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
    ButtonLabel->SetShadowOffset(FVector2D(1.5f, 1.5f));
    ButtonLabel->SetVisibility(ESlateVisibility::HitTestInvisible);

    FSlateFontInfo LabelFont = ButtonLabel->GetFont();
    LabelFont.Size = 21;
    ButtonLabel->SetFont(LabelFont);
    ActionButton->AddChild(ButtonLabel);

    if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(ButtonLabel->Slot))
    {
        LabelSlot->SetHorizontalAlignment(HAlign_Center);
        LabelSlot->SetVerticalAlignment(VAlign_Center);
        LabelSlot->SetPadding(FMargin(8.0f));
    }

    if (UCanvasPanelSlot* ButtonSlot = Parent->AddChildToCanvas(ActionButton))
    {
        ButtonSlot->SetAnchors(FAnchors(1.0f, 1.0f));
        ButtonSlot->SetAlignment(FVector2D(1.0f, 1.0f));
        ButtonSlot->SetPosition(Position);
        ButtonSlot->SetSize(FVector2D(160.0f, 112.0f));
    }

    return ActionButton;
}

void UGrandCityMobileControlsWidget::ShowFeedback(const FText& FeedbackText)
{
    if (!FeedbackLabel)
    {
        return;
    }

    FeedbackLabel->SetText(FeedbackText);
    FeedbackLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UGrandCityMobileControlsWidget::ClearFeedback()
{
    if (FeedbackLabel)
    {
        FeedbackLabel->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UGrandCityMobileControlsWidget::HandleRunPressed()
{
    OnRunPressed.ExecuteIfBound();
}

void UGrandCityMobileControlsWidget::HandleJumpPressed()
{
    OnJumpPressed.ExecuteIfBound();
}

void UGrandCityMobileControlsWidget::HandleCrouchPressed()
{
    OnCrouchPressed.ExecuteIfBound();
}
