#include "UI/GrandCityVehicleUpgradeWidget.h"

#include "Vehicles/GrandCityVehicleUpgradeStation.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"

namespace GrandCityVehicleUpgradeUI
{
    constexpr int32 SwatchColumns = 3;
    const FLinearColor TitleColor(0.3f, 1.0f, 0.4f);
    const FLinearColor AcceptColor(0.05f, 0.4f, 0.14f, 0.95f);
    const FLinearColor DeclineColor(0.45f, 0.06f, 0.06f, 0.95f);
    const FLinearColor NeutralColor(0.12f, 0.16f, 0.22f, 0.95f);
    const FLinearColor PaintButtonColor(0.05f, 0.22f, 0.5f, 0.95f);
    const FLinearColor SelectedFrameColor(1.0f, 1.0f, 1.0f, 1.0f);

    /** PC shows the keyboard shortcut (QuestAccept / QuestDecline in DefaultInput.ini). */
    FText WithKey(const FText& Label, const TCHAR* Key)
    {
        return PLATFORM_DESKTOP ? FText::Format(INVTEXT("{0} ({1})"), Label, FText::FromString(Key)) : Label;
    }
}

// --- Swatch button ----------------------------------------------------------------------------

UGrandCityPaintSwatchButton::UGrandCityPaintSwatchButton(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UGrandCityPaintSwatchButton::BindSwatch(int32 InPaintIndex, const FGrandCityPaintSelectedDelegate& InOnSelected)
{
    PaintIndex = InPaintIndex;
    OnSelected = InOnSelected;
    OnClicked.AddUniqueDynamic(this, &UGrandCityPaintSwatchButton::HandleClicked);
}

void UGrandCityPaintSwatchButton::HandleClicked()
{
    OnSelected.ExecuteIfBound(PaintIndex);
}

// --- Window -----------------------------------------------------------------------------------

void UGrandCityVehicleUpgradeWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!WidgetTree || WidgetTree->RootWidget)
    {
        return;
    }

    using namespace GrandCityVehicleUpgradeUI;

    UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WorkshopRoot"));
    RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    WidgetTree->RootWidget = RootCanvas;

    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("WorkshopPanel"));
    Panel->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.94f));
    Panel->SetPadding(FMargin(32.0f, 28.0f));
    if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel))
    {
        PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
        PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        PanelSlot->SetAutoSize(true);
    }

    USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("WorkshopSize"));
    SizeBox->SetWidthOverride(620.0f);
    SizeBox->SetMaxDesiredHeight(900.0f);
    Panel->SetContent(SizeBox);

    PageSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("WorkshopPages"));
    SizeBox->SetContent(PageSwitcher);

    // Menu: the three choices stacked, so each is a wide touch target.
    {
        UVerticalBox* Page = CreatePage(TEXT("MenuPage"));
        MenuTitleText = CreateText(TEXT("MenuTitle"), 34, TitleColor);
        AddRow(Page, MenuTitleText, 8.0f);
        UTextBlock* Subtitle = CreateText(TEXT("MenuSubtitle"), 20, FLinearColor::White);
        Subtitle->SetText(NSLOCTEXT("GrandCityVehicleUpgrade", "MenuSubtitle", "What would you like to do with your vehicle?"));
        AddRow(Page, Subtitle, 24.0f);

        UButton* PaintButton = CreateButton(TEXT("MenuPaintButton"),
            NSLOCTEXT("GrandCityVehicleUpgrade", "MenuPaint", "CHANGE COLOR"), PaintButtonColor);
        PaintButton->OnClicked.AddDynamic(this, &UGrandCityVehicleUpgradeWidget::HandleMenuPaintClicked);
        AddRow(Page, PaintButton, 12.0f);

        MenuUpgradeButton = CreateButton(TEXT("MenuUpgradeButton"),
            NSLOCTEXT("GrandCityVehicleUpgrade", "MenuUpgrade", "UPGRADE"), AcceptColor);
        MenuUpgradeButton->OnClicked.AddDynamic(this, &UGrandCityVehicleUpgradeWidget::HandleMenuUpgradeClicked);
        MenuUpgradeLabel = Cast<UTextBlock>(MenuUpgradeButton->GetContent());
        AddRow(Page, MenuUpgradeButton, 12.0f);

        UButton* CloseButton = CreateButton(TEXT("MenuCloseButton"),
            WithKey(NSLOCTEXT("GrandCityVehicleUpgrade", "MenuClose", "CLOSE"), TEXT("U")), DeclineColor);
        CloseButton->OnClicked.AddDynamic(this, &UGrandCityVehicleUpgradeWidget::HandleMenuCloseClicked);
        AddRow(Page, CloseButton, 0.0f);
    }

    // Upgrade offer: same layout as the quest offer window.
    {
        UVerticalBox* Page = CreatePage(TEXT("UpgradePage"));
        UpgradeTitleText = CreateText(TEXT("UpgradeTitle"), 34, TitleColor);
        UpgradeQuestionText = CreateText(TEXT("UpgradeQuestion"), 24, FLinearColor::White);
        UpgradeDetailsText = CreateText(TEXT("UpgradeDetails"), 17, FLinearColor(0.65f, 0.65f, 0.65f));
        AddRow(Page, UpgradeTitleText, 12.0f);
        AddRow(Page, UpgradeQuestionText, 16.0f);
        AddRow(Page, UpgradeDetailsText, 24.0f);

        UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("UpgradeButtons"));
        AddRow(Page, ButtonRow, 0.0f);
        UButton* AcceptButton = CreateButton(TEXT("UpgradeAcceptButton"),
            WithKey(NSLOCTEXT("GrandCityVehicleUpgrade", "Accept", "ACCEPT"), TEXT("Y")), AcceptColor);
        AcceptButton->OnClicked.AddDynamic(this, &UGrandCityVehicleUpgradeWidget::HandleUpgradeAcceptClicked);
        AddButtonToRow(ButtonRow, AcceptButton);
        UButton* DeclineButton = CreateButton(TEXT("UpgradeDeclineButton"),
            WithKey(NSLOCTEXT("GrandCityVehicleUpgrade", "Decline", "DECLINE"), TEXT("U")), DeclineColor);
        DeclineButton->OnClicked.AddDynamic(this, &UGrandCityVehicleUpgradeWidget::HandleBackClicked);
        AddButtonToRow(ButtonRow, DeclineButton);
    }

    // Change color: swatch grid, filled by SetStation.
    {
        UVerticalBox* Page = CreatePage(TEXT("PaintPage"));
        UTextBlock* Title = CreateText(TEXT("PaintTitle"), 34, TitleColor);
        Title->SetText(NSLOCTEXT("GrandCityVehicleUpgrade", "PaintTitle", "CHANGE COLOR"));
        AddRow(Page, Title, 8.0f);
        PaintCurrentText = CreateText(TEXT("PaintCurrent"), 20, FLinearColor::White);
        AddRow(Page, PaintCurrentText, 16.0f);

        SwatchGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("PaintSwatches"));
        SwatchGrid->SetSlotPadding(FMargin(6.0f));
        AddRow(Page, SwatchGrid, 20.0f);

        UButton* BackButton = CreateButton(TEXT("PaintBackButton"),
            WithKey(NSLOCTEXT("GrandCityVehicleUpgrade", "Back", "BACK"), TEXT("U")), NeutralColor);
        BackButton->OnClicked.AddDynamic(this, &UGrandCityVehicleUpgradeWidget::HandleBackClicked);
        AddRow(Page, BackButton, 0.0f);
    }

    ShowPage(EPage::Menu);
}

UVerticalBox* UGrandCityVehicleUpgradeWidget::CreatePage(FName Name)
{
    UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), Name);
    PageSwitcher->AddChild(Page);
    return Page;
}

void UGrandCityVehicleUpgradeWidget::AddRow(UVerticalBox* Page, UWidget* Row, float BottomPadding)
{
    if (UVerticalBoxSlot* RowSlot = Page->AddChildToVerticalBox(Row))
    {
        RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
    }
}

void UGrandCityVehicleUpgradeWidget::AddButtonToRow(UPanelWidget* Row, UButton* Button)
{
    const bool bFirst = Row->GetChildrenCount() == 0;
    if (UHorizontalBoxSlot* ButtonSlot = Cast<UHorizontalBoxSlot>(Row->AddChild(Button)))
    {
        ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        ButtonSlot->SetPadding(FMargin(bFirst ? 0.0f : 16.0f, 0.0f, 0.0f, 0.0f));
    }
}

UTextBlock* UGrandCityVehicleUpgradeWidget::CreateText(FName Name, int32 FontSize, const FLinearColor& Color)
{
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
    Text->SetColorAndOpacity(FSlateColor(Color));
    Text->SetAutoWrapText(true);
    Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
    Text->SetShadowOffset(FVector2D(1.0f, 1.0f));

    FSlateFontInfo Font = Text->GetFont();
    Font.Size = FontSize;
    Text->SetFont(Font);
    return Text;
}

UButton* UGrandCityVehicleUpgradeWidget::CreateButton(FName Name, const FText& Label, const FLinearColor& Color, int32 FontSize)
{
    UButton* Button = WidgetTree->ConstructWidget<UGrandCityMobileActionButton>(
        UGrandCityMobileActionButton::StaticClass(), Name);
    Button->SetBackgroundColor(Color);
    // Fire on release so a touch that slides off the button cancels.
    Button->SetTouchMethod(EButtonTouchMethod::DownAndUp);

    UTextBlock* ButtonLabel = CreateText(*FString::Printf(TEXT("%sLabel"), *Name.ToString()), FontSize, FLinearColor::White);
    ButtonLabel->SetText(Label);
    ButtonLabel->SetAutoWrapText(false);
    ButtonLabel->SetJustification(ETextJustify::Center);
    Button->AddChild(ButtonLabel);
    if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(ButtonLabel->Slot))
    {
        LabelSlot->SetHorizontalAlignment(HAlign_Center);
        LabelSlot->SetVerticalAlignment(VAlign_Center);
        LabelSlot->SetPadding(FMargin(12.0f, 18.0f));
    }
    return Button;
}

void UGrandCityVehicleUpgradeWidget::SetStation(const FText& StationName, const FText& InUpgradeName,
    const FText& UpgradeQuestion, const FText& UpgradeDetails, const TArray<FGrandCityPaintOption>& Palette)
{
    if (!PageSwitcher)
    {
        return;
    }

    MenuTitleText->SetText(StationName);
    UpgradeName = InUpgradeName;
    UpgradeTitleText->SetText(InUpgradeName);
    UpgradeQuestionText->SetText(UpgradeQuestion);
    UpgradeDetailsText->SetText(UpgradeDetails);
    UpgradeDetailsText->SetVisibility(UpgradeDetails.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    SetUpgradeAvailable(bUpgradeAvailable);

    SwatchGrid->ClearChildren();
    SwatchFrames.Reset();
    PaintNames.Reset();
    const FGrandCityPaintSelectedDelegate OnSwatch =
        FGrandCityPaintSelectedDelegate::CreateUObject(this, &UGrandCityVehicleUpgradeWidget::HandleSwatchSelected);
    for (int32 Index = 0; Index < Palette.Num(); ++Index)
    {
        const FGrandCityPaintOption& Option = Palette[Index];
        PaintNames.Add(Option.Name);

        UGrandCityPaintSwatchButton* Swatch = WidgetTree->ConstructWidget<UGrandCityPaintSwatchButton>(
            UGrandCityPaintSwatchButton::StaticClass());
        Swatch->SetTouchMethod(EButtonTouchMethod::DownAndUp);
        // A flat rounded brush of the paint colour itself (the default style would tint it grey).
        const FLinearColor Opaque(Option.Color.R, Option.Color.G, Option.Color.B, 1.0f);
        FButtonStyle Style = Swatch->GetStyle();
        Style.SetNormal(FSlateRoundedBoxBrush(Opaque, 8.0f));
        Style.SetHovered(FSlateRoundedBoxBrush(FMath::Lerp(Opaque, FLinearColor::White, 0.25f), 8.0f));
        Style.SetPressed(FSlateRoundedBoxBrush(FMath::Lerp(Opaque, FLinearColor::Black, 0.4f), 8.0f));
        Style.SetNormalPadding(FMargin(0.0f));
        Style.SetPressedPadding(FMargin(0.0f));
        Swatch->SetStyle(Style);
        Swatch->BindSwatch(Index, OnSwatch);

        // Dark text on light paint, white text on dark paint.
        const bool bLightPaint = Option.Color.GetLuminance() > 0.3f;
        UTextBlock* SwatchLabel = CreateText(NAME_None, 18, bLightPaint ? FLinearColor(0.02f, 0.02f, 0.02f) : FLinearColor::White);
        SwatchLabel->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, bLightPaint ? 0.0f : 0.8f));
        SwatchLabel->SetText(Option.Name);
        SwatchLabel->SetAutoWrapText(false);
        SwatchLabel->SetJustification(ETextJustify::Center);
        Swatch->AddChild(SwatchLabel);
        if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(SwatchLabel->Slot))
        {
            LabelSlot->SetHorizontalAlignment(HAlign_Center);
            LabelSlot->SetVerticalAlignment(VAlign_Center);
        }

        USizeBox* SwatchSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        SwatchSize->SetHeightOverride(84.0f);
        SwatchSize->SetContent(Swatch);

        UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Frame->SetPadding(FMargin(4.0f));
        Frame->SetBrushColor(FLinearColor::Transparent);
        Frame->SetContent(SwatchSize);
        SwatchFrames.Add(Frame);

        if (UUniformGridSlot* GridSlot = SwatchGrid->AddChildToUniformGrid(
            Frame, Index / GrandCityVehicleUpgradeUI::SwatchColumns, Index % GrandCityVehicleUpgradeUI::SwatchColumns))
        {
            GridSlot->SetHorizontalAlignment(HAlign_Fill);
            GridSlot->SetVerticalAlignment(VAlign_Fill);
        }
    }

    SetSelectedPaint(INDEX_NONE);
    ShowPage(EPage::Menu);
}

void UGrandCityVehicleUpgradeWidget::SetUpgradeAvailable(bool bAvailable)
{
    bUpgradeAvailable = bAvailable;
    if (!MenuUpgradeButton)
    {
        return;
    }

    MenuUpgradeButton->SetIsEnabled(bAvailable);
    if (MenuUpgradeLabel)
    {
        const FText Label = bAvailable
            ? FText::Format(NSLOCTEXT("GrandCityVehicleUpgrade", "MenuUpgradeName", "UPGRADE: {0}"), UpgradeName)
            : FText::Format(NSLOCTEXT("GrandCityVehicleUpgrade", "MenuUpgradeInstalled", "{0} INSTALLED"), UpgradeName);
        if (!MenuUpgradeLabel->GetText().EqualTo(Label))
        {
            MenuUpgradeLabel->SetText(Label);
        }
    }
    if (!bAvailable && CurrentPage == EPage::Upgrade)
    {
        ShowPage(EPage::Menu);
    }
}

void UGrandCityVehicleUpgradeWidget::SetSelectedPaint(int32 PaintIndex)
{
    for (int32 Index = 0; Index < SwatchFrames.Num(); ++Index)
    {
        SwatchFrames[Index]->SetBrushColor(Index == PaintIndex
            ? GrandCityVehicleUpgradeUI::SelectedFrameColor
            : FLinearColor::Transparent);
    }

    if (PaintCurrentText)
    {
        PaintCurrentText->SetText(PaintNames.IsValidIndex(PaintIndex)
            ? FText::Format(NSLOCTEXT("GrandCityVehicleUpgrade", "PaintCurrent", "Current color: {0}"), PaintNames[PaintIndex])
            : NSLOCTEXT("GrandCityVehicleUpgrade", "PaintFactory", "Current color: Factory paint"));
    }
}

void UGrandCityVehicleUpgradeWidget::ShowPage(EPage Page)
{
    CurrentPage = Page;
    if (PageSwitcher)
    {
        PageSwitcher->SetActiveWidgetIndex(static_cast<int32>(Page));
    }
}

bool UGrandCityVehicleUpgradeWidget::HandleAcceptKey()
{
    if (CurrentPage != EPage::Upgrade)
    {
        return false;
    }
    HandleUpgradeAcceptClicked();
    return true;
}

void UGrandCityVehicleUpgradeWidget::HandleBackKey()
{
    if (CurrentPage == EPage::Menu)
    {
        HandleMenuCloseClicked();
    }
    else
    {
        HandleBackClicked();
    }
}

void UGrandCityVehicleUpgradeWidget::HandleSwatchSelected(int32 PaintIndex)
{
    SetSelectedPaint(PaintIndex);
    OnPaintSelected.ExecuteIfBound(PaintIndex);
}

void UGrandCityVehicleUpgradeWidget::HandleMenuPaintClicked()
{
    ShowPage(EPage::Paint);
}

void UGrandCityVehicleUpgradeWidget::HandleMenuUpgradeClicked()
{
    if (bUpgradeAvailable)
    {
        ShowPage(EPage::Upgrade);
    }
}

void UGrandCityVehicleUpgradeWidget::HandleMenuCloseClicked()
{
    OnClosed.ExecuteIfBound();
}

void UGrandCityVehicleUpgradeWidget::HandleUpgradeAcceptClicked()
{
    OnUpgradeAccepted.ExecuteIfBound();
}

void UGrandCityVehicleUpgradeWidget::HandleBackClicked()
{
    ShowPage(EPage::Menu);
}
