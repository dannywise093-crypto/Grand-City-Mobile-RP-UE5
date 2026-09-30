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

    EnterVehicleButton = CreateActionButton(
        RootCanvas,
        TEXT("EnterVehicleButton"),
        NSLOCTEXT("GrandCityMobileControls", "EnterVehicleButton", "ENTER"),
        FVector2D(-208.0f, -160.0f));

    // Above ENTER so a quest giver next to a parked car keeps both actions reachable.
    QuestInteractButton = CreateActionButton(
        RootCanvas,
        TEXT("QuestInteractButton"),
        NSLOCTEXT("GrandCityMobileControls", "QuestInteractButton", "QUEST"),
        FVector2D(-208.0f, -288.0f));

    if (RunButton)
    {
        RunButton->OnClicked.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleRunPressed);
        CharacterButtons.Add(RunButton);
    }
    if (JumpButton)
    {
        JumpButton->OnClicked.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleJumpPressed);
        CharacterButtons.Add(JumpButton);
    }
    if (CrouchButton)
    {
        CrouchButton->OnClicked.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleCrouchPressed);
        CharacterButtons.Add(CrouchButton);
    }
    if (EnterVehicleButton)
    {
        EnterVehicleButton->SetBackgroundColor(FLinearColor(0.05f, 0.35f, 0.12f, 0.9f));
        EnterVehicleButton->OnClicked.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleEnterVehiclePressed);
    }
    if (QuestInteractButton)
    {
        QuestInteractButton->SetBackgroundColor(FLinearColor(0.45f, 0.32f, 0.02f, 0.9f));
        QuestInteractButton->OnClicked.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleQuestInteractPressed);
    }

    // Driving layout. Pedals sit bottom-right (FORWARD above REVERSE, a tall BRAKE
    // pedal beside them, EXIT kept apart at the top); steering sits bottom-left.
    const FVector2D BottomLeft(0.0f, 1.0f);
    const FVector2D BottomRight(1.0f, 1.0f);
    UButton* ForwardButton = CreateActionButton(
        RootCanvas, TEXT("VehicleForwardButton"),
        NSLOCTEXT("GrandCityMobileControls", "VehicleForwardButton", "FORWARD"),
        FVector2D(-32.0f, -160.0f), BottomRight, FVector2D(160.0f, 112.0f), true);
    UButton* ReverseButton = CreateActionButton(
        RootCanvas, TEXT("VehicleReverseButton"),
        NSLOCTEXT("GrandCityMobileControls", "VehicleReverseButton", "REVERSE"),
        FVector2D(-32.0f, -32.0f), BottomRight, FVector2D(160.0f, 112.0f), true);
    UButton* BrakeButton = CreateActionButton(
        RootCanvas, TEXT("VehicleBrakeButton"),
        NSLOCTEXT("GrandCityMobileControls", "VehicleBrakeButton", "BRAKE"),
        FVector2D(-208.0f, -32.0f), BottomRight, FVector2D(150.0f, 240.0f), true);
    UButton* ExitVehicleButton = CreateActionButton(
        RootCanvas, TEXT("VehicleExitButton"),
        NSLOCTEXT("GrandCityMobileControls", "VehicleExitButton", "EXIT"),
        FVector2D(-32.0f, -304.0f), BottomRight, FVector2D(160.0f, 80.0f));
    UButton* SteerLeftButton = CreateActionButton(
        RootCanvas, TEXT("VehicleSteerLeftButton"),
        NSLOCTEXT("GrandCityMobileControls", "VehicleSteerLeftButton", "LEFT"),
        FVector2D(32.0f, -32.0f), BottomLeft, FVector2D(170.0f, 150.0f), true);
    UButton* SteerRightButton = CreateActionButton(
        RootCanvas, TEXT("VehicleSteerRightButton"),
        NSLOCTEXT("GrandCityMobileControls", "VehicleSteerRightButton", "RIGHT"),
        FVector2D(218.0f, -32.0f), BottomLeft, FVector2D(170.0f, 150.0f), true);

    if (ForwardButton)
    {
        ForwardButton->OnPressed.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleForwardPressed);
        ForwardButton->OnReleased.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleForwardReleased);
        VehicleButtons.Add(ForwardButton);
    }
    if (ReverseButton)
    {
        ReverseButton->OnPressed.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleReversePressed);
        ReverseButton->OnReleased.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleReverseReleased);
        VehicleButtons.Add(ReverseButton);
    }
    if (BrakeButton)
    {
        BrakeButton->SetBackgroundColor(FLinearColor(0.35f, 0.05f, 0.05f, 0.85f));
        BrakeButton->OnPressed.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleBrakePressed);
        BrakeButton->OnReleased.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleBrakeReleased);
        VehicleButtons.Add(BrakeButton);
    }
    if (ExitVehicleButton)
    {
        ExitVehicleButton->OnClicked.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleExitVehiclePressed);
        VehicleButtons.Add(ExitVehicleButton);
    }
    if (SteerLeftButton)
    {
        SteerLeftButton->OnPressed.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleSteerLeftPressed);
        SteerLeftButton->OnReleased.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleSteerLeftReleased);
        VehicleButtons.Add(SteerLeftButton);
    }
    if (SteerRightButton)
    {
        SteerRightButton->OnPressed.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleSteerRightPressed);
        SteerRightButton->OnReleased.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleSteerRightReleased);
        VehicleButtons.Add(SteerRightButton);
    }

    // Driving extras, shown only when they apply: BOOST above the brake pedal (cars with
    // nitro), WORKSHOP above EXIT (parked at a vehicle workshop).
    BoostButton = CreateActionButton(
        RootCanvas, TEXT("VehicleBoostButton"),
        NSLOCTEXT("GrandCityMobileControls", "VehicleBoostButton", "BOOST"),
        FVector2D(-208.0f, -288.0f), BottomRight, FVector2D(150.0f, 112.0f), true);
    VehicleInteractButton = CreateActionButton(
        RootCanvas, TEXT("VehicleInteractButton"),
        NSLOCTEXT("GrandCityMobileControls", "VehicleInteractButton", "WORKSHOP"),
        FVector2D(-32.0f, -400.0f), BottomRight, FVector2D(160.0f, 80.0f));

    if (BoostButton)
    {
        BoostButton->SetBackgroundColor(FLinearColor(0.02f, 0.3f, 0.55f, 0.9f));
        BoostButton->OnPressed.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleBoostPressed);
        BoostButton->OnReleased.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleBoostReleased);
    }
    if (VehicleInteractButton)
    {
        VehicleInteractButton->SetBackgroundColor(FLinearColor(0.05f, 0.45f, 0.15f, 0.9f));
        VehicleInteractButton->OnClicked.AddDynamic(this, &UGrandCityMobileControlsWidget::HandleVehicleInteractPressed);
    }

    RefreshButtonVisibility();
}

void UGrandCityMobileControlsWidget::SetVehicleMode(bool bInVehicle)
{
    if (bVehicleMode == bInVehicle)
    {
        return;
    }

    bVehicleMode = bInVehicle;
    RefreshButtonVisibility();
}

void UGrandCityMobileControlsWidget::SetEnterVehicleAvailable(bool bAvailable)
{
    if (bEnterVehicleAvailable == bAvailable)
    {
        return;
    }

    bEnterVehicleAvailable = bAvailable;
    RefreshButtonVisibility();
}

void UGrandCityMobileControlsWidget::SetButtonLabel(UButton* Button, const FText& Label)
{
    if (UTextBlock* ButtonLabel = Button ? Cast<UTextBlock>(Button->GetContent()) : nullptr)
    {
        if (!ButtonLabel->GetText().EqualTo(Label))
        {
            ButtonLabel->SetText(Label);
        }
    }
}

void UGrandCityMobileControlsWidget::SetQuestInteractAvailable(bool bAvailable, const FText& Label)
{
    if (bAvailable)
    {
        SetButtonLabel(QuestInteractButton, Label);
    }

    if (bQuestInteractAvailable == bAvailable)
    {
        return;
    }

    bQuestInteractAvailable = bAvailable;
    RefreshButtonVisibility();
}

void UGrandCityMobileControlsWidget::SetVehicleInteractAvailable(bool bAvailable, const FText& Label)
{
    if (bAvailable)
    {
        SetButtonLabel(VehicleInteractButton, Label);
    }

    if (bVehicleInteractAvailable == bAvailable)
    {
        return;
    }

    bVehicleInteractAvailable = bAvailable;
    RefreshButtonVisibility();
}

void UGrandCityMobileControlsWidget::SetBoostAvailable(bool bAvailable)
{
    if (bBoostAvailable == bAvailable)
    {
        return;
    }

    bBoostAvailable = bAvailable;
    RefreshButtonVisibility();
}

void UGrandCityMobileControlsWidget::RefreshButtonVisibility()
{
    // The controller clears held pedal/steering input on every mode switch, so a
    // button collapsed mid-press cannot leave the vehicle input latched.
    for (UButton* Button : CharacterButtons)
    {
        Button->SetVisibility(bVehicleMode ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    }
    for (UButton* Button : VehicleButtons)
    {
        Button->SetVisibility(bVehicleMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    if (EnterVehicleButton)
    {
        EnterVehicleButton->SetVisibility(!bVehicleMode && bEnterVehicleAvailable
            ? ESlateVisibility::Visible
            : ESlateVisibility::Collapsed);
    }
    if (QuestInteractButton)
    {
        QuestInteractButton->SetVisibility(!bVehicleMode && bQuestInteractAvailable
            ? ESlateVisibility::Visible
            : ESlateVisibility::Collapsed);
    }
    if (VehicleInteractButton)
    {
        VehicleInteractButton->SetVisibility(bVehicleMode && bVehicleInteractAvailable
            ? ESlateVisibility::Visible
            : ESlateVisibility::Collapsed);
    }
    if (BoostButton)
    {
        BoostButton->SetVisibility(bVehicleMode && bBoostAvailable
            ? ESlateVisibility::Visible
            : ESlateVisibility::Collapsed);
    }
}

UButton* UGrandCityMobileControlsWidget::CreateActionButton(
    UCanvasPanel* Parent,
    FName ButtonName,
    const FText& Label,
    const FVector2D& Position,
    const FVector2D& Anchor,
    const FVector2D& Size,
    bool bHoldButton)
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
    // Held buttons (pedals, steering) instead capture their own finger so the
    // release always arrives, even if the finger slides off the button.
    ActionButton->SetTouchMethod(bHoldButton ? EButtonTouchMethod::DownAndUp : EButtonTouchMethod::Down);

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
        ButtonSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
        ButtonSlot->SetAlignment(Anchor);
        ButtonSlot->SetPosition(Position);
        ButtonSlot->SetSize(Size);
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

void UGrandCityMobileControlsWidget::HandleEnterVehiclePressed()
{
    OnEnterVehiclePressed.ExecuteIfBound();
}

void UGrandCityMobileControlsWidget::HandleExitVehiclePressed()
{
    OnExitVehiclePressed.ExecuteIfBound();
}

void UGrandCityMobileControlsWidget::HandleQuestInteractPressed()
{
    OnQuestInteractPressed.ExecuteIfBound();
}

void UGrandCityMobileControlsWidget::HandleForwardPressed()
{
    OnVehicleControlChanged.ExecuteIfBound(EGrandCityVehicleControl::Forward, true);
}

void UGrandCityMobileControlsWidget::HandleForwardReleased()
{
    OnVehicleControlChanged.ExecuteIfBound(EGrandCityVehicleControl::Forward, false);
}

void UGrandCityMobileControlsWidget::HandleBrakePressed()
{
    OnVehicleControlChanged.ExecuteIfBound(EGrandCityVehicleControl::Brake, true);
}

void UGrandCityMobileControlsWidget::HandleBrakeReleased()
{
    OnVehicleControlChanged.ExecuteIfBound(EGrandCityVehicleControl::Brake, false);
}

void UGrandCityMobileControlsWidget::HandleReversePressed()
{
    OnVehicleControlChanged.ExecuteIfBound(EGrandCityVehicleControl::Reverse, true);
}

void UGrandCityMobileControlsWidget::HandleReverseReleased()
{
    OnVehicleControlChanged.ExecuteIfBound(EGrandCityVehicleControl::Reverse, false);
}

void UGrandCityMobileControlsWidget::HandleSteerLeftPressed()
{
    OnVehicleControlChanged.ExecuteIfBound(EGrandCityVehicleControl::SteerLeft, true);
}

void UGrandCityMobileControlsWidget::HandleSteerLeftReleased()
{
    OnVehicleControlChanged.ExecuteIfBound(EGrandCityVehicleControl::SteerLeft, false);
}

void UGrandCityMobileControlsWidget::HandleSteerRightPressed()
{
    OnVehicleControlChanged.ExecuteIfBound(EGrandCityVehicleControl::SteerRight, true);
}

void UGrandCityMobileControlsWidget::HandleSteerRightReleased()
{
    OnVehicleControlChanged.ExecuteIfBound(EGrandCityVehicleControl::SteerRight, false);
}

void UGrandCityMobileControlsWidget::HandleBoostPressed()
{
    OnVehicleControlChanged.ExecuteIfBound(EGrandCityVehicleControl::Boost, true);
}

void UGrandCityMobileControlsWidget::HandleBoostReleased()
{
    OnVehicleControlChanged.ExecuteIfBound(EGrandCityVehicleControl::Boost, false);
}

void UGrandCityMobileControlsWidget::HandleVehicleInteractPressed()
{
    OnVehicleInteractPressed.ExecuteIfBound();
}
