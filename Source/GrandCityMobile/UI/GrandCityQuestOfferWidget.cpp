#include "UI/GrandCityQuestOfferWidget.h"

#include "Quests/GrandCityQuestTypes.h"
#include "UI/GrandCityMobileControlsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace GrandCityQuestOffer
{
    FText ObjectiveTypeName(EGrandCityQuestObjectiveType Type)
    {
        switch (Type)
        {
        case EGrandCityQuestObjectiveType::ReachLocation: return NSLOCTEXT("GrandCityQuest", "TypeReach", "Go to");
        case EGrandCityQuestObjectiveType::Deliver: return NSLOCTEXT("GrandCityQuest", "TypeDeliver", "Delivery");
        case EGrandCityQuestObjectiveType::Collect: return NSLOCTEXT("GrandCityQuest", "TypeCollect", "Collect");
        case EGrandCityQuestObjectiveType::Destroy: return NSLOCTEXT("GrandCityQuest", "TypeDestroy", "Destroy");
        case EGrandCityQuestObjectiveType::Interact: return NSLOCTEXT("GrandCityQuest", "TypeInteract", "Interact");
        default: return NSLOCTEXT("GrandCityQuest", "TypeCustom", "Task");
        }
    }
}

FString UGrandCityQuestOfferWidget::FormatTime(float Seconds)
{
    const int32 TotalSeconds = FMath::Max(FMath::CeilToInt(Seconds), 0);
    return FString::Printf(TEXT("%d:%02d"), TotalSeconds / 60, TotalSeconds % 60);
}

void UGrandCityQuestOfferWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!WidgetTree || WidgetTree->RootWidget)
    {
        return;
    }

    UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("QuestOfferRoot"));
    RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    WidgetTree->RootWidget = RootCanvas;

    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("QuestOfferPanel"));
    Panel->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.94f));
    Panel->SetPadding(FMargin(32.0f, 28.0f));
    if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel))
    {
        PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
        PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        PanelSlot->SetAutoSize(true);
    }

    USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("QuestOfferSize"));
    SizeBox->SetWidthOverride(680.0f);
    SizeBox->SetMaxDesiredHeight(900.0f);
    Panel->SetContent(SizeBox);

    UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("QuestOfferColumn"));
    SizeBox->SetContent(Column);

    TitleText = CreateText(TEXT("QuestOfferTitle"), 34, FLinearColor(1.0f, 0.82f, 0.15f));
    DescriptionText = CreateText(TEXT("QuestOfferDescription"), 20, FLinearColor::White);
    UTextBlock* ObjectivesHeader = CreateText(TEXT("QuestOfferObjectivesHeader"), 18, FLinearColor(0.55f, 0.75f, 1.0f));
    ObjectivesHeader->SetText(NSLOCTEXT("GrandCityQuest", "OfferObjectivesHeader", "OBJECTIVES"));
    ObjectivesText = CreateText(TEXT("QuestOfferObjectives"), 19, FLinearColor(0.9f, 0.9f, 0.9f));
    RulesText = CreateText(TEXT("QuestOfferRules"), 16, FLinearColor(0.65f, 0.65f, 0.65f));

    const TPair<UTextBlock*, float> Rows[] = {
        {TitleText, 12.0f}, {DescriptionText, 20.0f}, {ObjectivesHeader, 6.0f}, {ObjectivesText, 14.0f}, {RulesText, 24.0f}};
    for (const TPair<UTextBlock*, float>& Row : Rows)
    {
        if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row.Key))
        {
            RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, Row.Value));
        }
    }

    UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("QuestOfferButtons"));
    Column->AddChildToVerticalBox(ButtonRow);

    // PC shows the keyboard shortcuts (QuestAccept / QuestDecline in DefaultInput.ini).
    const FText AcceptLabel = PLATFORM_DESKTOP
        ? NSLOCTEXT("GrandCityQuest", "AcceptKey", "ACCEPT (Y)") : NSLOCTEXT("GrandCityQuest", "Accept", "ACCEPT");
    const FText DeclineLabel = PLATFORM_DESKTOP
        ? NSLOCTEXT("GrandCityQuest", "DeclineKey", "DECLINE (U)") : NSLOCTEXT("GrandCityQuest", "Decline", "DECLINE");

    if (UButton* AcceptButton = CreateButton(ButtonRow, TEXT("QuestAcceptButton"),
        AcceptLabel, FLinearColor(0.05f, 0.4f, 0.14f, 0.95f)))
    {
        AcceptButton->OnClicked.AddDynamic(this, &UGrandCityQuestOfferWidget::HandleAcceptClicked);
    }
    if (UButton* DeclineButton = CreateButton(ButtonRow, TEXT("QuestDeclineButton"),
        DeclineLabel, FLinearColor(0.45f, 0.06f, 0.06f, 0.95f)))
    {
        DeclineButton->OnClicked.AddDynamic(this, &UGrandCityQuestOfferWidget::HandleDeclineClicked);
    }
}

UTextBlock* UGrandCityQuestOfferWidget::CreateText(FName Name, int32 FontSize, const FLinearColor& Color)
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

UButton* UGrandCityQuestOfferWidget::CreateButton(
    UHorizontalBox* Parent, FName Name, const FText& Label, const FLinearColor& Color)
{
    UButton* Button = WidgetTree->ConstructWidget<UGrandCityMobileActionButton>(
        UGrandCityMobileActionButton::StaticClass(), Name);
    Button->SetBackgroundColor(Color);
    // Fire on release so a touch that slides off the button cancels.
    Button->SetTouchMethod(EButtonTouchMethod::DownAndUp);

    UTextBlock* ButtonLabel = CreateText(*FString::Printf(TEXT("%sLabel"), *Name.ToString()), 24, FLinearColor::White);
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

    if (UHorizontalBoxSlot* ButtonSlot = Parent->AddChildToHorizontalBox(Button))
    {
        ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        ButtonSlot->SetPadding(FMargin(Parent->GetChildrenCount() > 1 ? 16.0f : 0.0f, 0.0f, 0.0f, 0.0f));
    }
    return Button;
}

void UGrandCityQuestOfferWidget::SetQuest(const FGrandCityQuestDefinition& Quest)
{
    if (!TitleText)
    {
        return;
    }

    TitleText->SetText(Quest.Title.IsEmpty() ? NSLOCTEXT("GrandCityQuest", "UntitledQuest", "Quest") : Quest.Title);
    DescriptionText->SetText(Quest.Description);
    DescriptionText->SetVisibility(Quest.Description.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

    FString Objectives;
    for (int32 Index = 0; Index < Quest.Objectives.Num(); ++Index)
    {
        const FGrandCityQuestObjective& Objective = Quest.Objectives[Index];
        FString Line = FString::Printf(TEXT("%d. [%s] %s"), Index + 1,
            *GrandCityQuestOffer::ObjectiveTypeName(Objective.Type).ToString(), *Objective.Description.ToString());
        if (Quest.bUseObjectiveTimers && Objective.bUseTimer)
        {
            Line += FString::Printf(TEXT("  (%s)"), *FormatTime(Objective.TimeLimitSeconds));
        }
        Objectives += (Index > 0 ? TEXT("\n") : TEXT("")) + Line;
    }
    ObjectivesText->SetText(FText::FromString(Objectives));

    FText Rules = Quest.Objectives.Num() > 1
        ? (Quest.bCompleteObjectivesInOrder
            ? NSLOCTEXT("GrandCityQuest", "RulesOrdered", "Objectives unlock one at a time, from top to bottom.")
            : NSLOCTEXT("GrandCityQuest", "RulesParallel", "All objectives are active at the same time."))
        : FText::GetEmpty();
    if (Quest.bUseQuestTimer)
    {
        const FText TimeLimit = FText::Format(NSLOCTEXT("GrandCityQuest", "RulesQuestTimer", "Time limit: {0}"),
            FText::FromString(FormatTime(Quest.QuestTimeLimitSeconds)));
        Rules = Rules.IsEmpty() ? TimeLimit : FText::Format(INVTEXT("{0}\n{1}"), Rules, TimeLimit);
    }
    RulesText->SetText(Rules);
    RulesText->SetVisibility(Rules.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UGrandCityQuestOfferWidget::HandleAcceptClicked()
{
    OnAccepted.ExecuteIfBound();
}

void UGrandCityQuestOfferWidget::HandleDeclineClicked()
{
    OnDeclined.ExecuteIfBound();
}
