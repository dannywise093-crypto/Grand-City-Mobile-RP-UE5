#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GrandCityQuestOfferWidget.generated.h"

class UButton;
class UHorizontalBox;
class UTextBlock;
struct FGrandCityQuestDefinition;

/** Quest offer window: title, description, objective list and ACCEPT / DECLINE buttons. */
UCLASS()
class GRANDCITYMOBILE_API UGrandCityQuestOfferWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    FSimpleDelegate OnAccepted;
    FSimpleDelegate OnDeclined;

    void SetQuest(const FGrandCityQuestDefinition& Quest);

    /** "1:05" style countdown text. */
    static FString FormatTime(float Seconds);

protected:
    virtual void NativeOnInitialized() override;

private:
    UTextBlock* CreateText(FName Name, int32 FontSize, const FLinearColor& Color);
    UButton* CreateButton(UHorizontalBox* Parent, FName Name, const FText& Label, const FLinearColor& Color);

    UFUNCTION()
    void HandleAcceptClicked();

    UFUNCTION()
    void HandleDeclineClicked();

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> DescriptionText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ObjectivesText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> RulesText;
};
