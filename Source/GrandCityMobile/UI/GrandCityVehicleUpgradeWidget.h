#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/GrandCityMobileControlsWidget.h"
#include "GrandCityVehicleUpgradeWidget.generated.h"

class UBorder;
class UButton;
class UPanelWidget;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;
class UWidget;
class UWidgetSwitcher;
struct FGrandCityPaintOption;

DECLARE_DELEGATE_OneParam(FGrandCityPaintSelectedDelegate, int32 /* PaintIndex */);

/** Colour swatch in the Change Color page; reports which palette entry it stands for. */
UCLASS()
class GRANDCITYMOBILE_API UGrandCityPaintSwatchButton : public UGrandCityMobileActionButton
{
    GENERATED_BODY()

public:
    explicit UGrandCityPaintSwatchButton(const FObjectInitializer& ObjectInitializer);

    void BindSwatch(int32 InPaintIndex, const FGrandCityPaintSelectedDelegate& InOnSelected);

private:
    UFUNCTION()
    void HandleClicked();

    int32 PaintIndex = INDEX_NONE;
    FGrandCityPaintSelectedDelegate OnSelected;
};

/**
 * Vehicle workshop window. Opens on a menu (CHANGE COLOR / UPGRADE / CLOSE); UPGRADE shows
 * the upgrade offer (ACCEPT / DECLINE), CHANGE COLOR shows the paint palette. Moving between
 * pages is handled here; the owner only hears about purchases and closing.
 */
UCLASS()
class GRANDCITYMOBILE_API UGrandCityVehicleUpgradeWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    FSimpleDelegate OnUpgradeAccepted;
    FGrandCityPaintSelectedDelegate OnPaintSelected;
    /** CLOSE on the menu (or the back key there). */
    FSimpleDelegate OnClosed;

    /** Fills every page and opens the menu. */
    void SetStation(const FText& StationName, const FText& UpgradeName, const FText& UpgradeQuestion,
        const FText& UpgradeDetails, const TArray<FGrandCityPaintOption>& Palette);
    /** Greys out the UPGRADE button once the vehicle has the upgrade. */
    void SetUpgradeAvailable(bool bAvailable);
    /** Frames the swatch of the vehicle's current colour (INDEX_NONE for none). */
    void SetSelectedPaint(int32 PaintIndex);

    /** Y key: accepts on the upgrade page. Returns false on the other pages. */
    bool HandleAcceptKey();
    /** U key: back to the menu from a sub page, close from the menu. */
    void HandleBackKey();

protected:
    virtual void NativeOnInitialized() override;

private:
    enum class EPage : int32
    {
        Menu,
        Upgrade,
        Paint
    };

    void ShowPage(EPage Page);
    UVerticalBox* CreatePage(FName Name);
    UTextBlock* CreateText(FName Name, int32 FontSize, const FLinearColor& Color);
    UButton* CreateButton(FName Name, const FText& Label, const FLinearColor& Color, int32 FontSize = 24);
    void AddRow(UVerticalBox* Page, UWidget* Row, float BottomPadding);
    void AddButtonToRow(UPanelWidget* Row, UButton* Button);
    void HandleSwatchSelected(int32 PaintIndex);

    UFUNCTION()
    void HandleMenuPaintClicked();

    UFUNCTION()
    void HandleMenuUpgradeClicked();

    UFUNCTION()
    void HandleMenuCloseClicked();

    UFUNCTION()
    void HandleUpgradeAcceptClicked();

    UFUNCTION()
    void HandleBackClicked();

    UPROPERTY(Transient)
    TObjectPtr<UWidgetSwitcher> PageSwitcher;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> MenuTitleText;

    UPROPERTY(Transient)
    TObjectPtr<UButton> MenuUpgradeButton;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> MenuUpgradeLabel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> UpgradeTitleText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> UpgradeQuestionText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> UpgradeDetailsText;

    UPROPERTY(Transient)
    TObjectPtr<UUniformGridPanel> SwatchGrid;

    /** Selection frame around each swatch, by palette index. */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UBorder>> SwatchFrames;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> PaintCurrentText;

    TArray<FText> PaintNames;
    FText UpgradeName;
    EPage CurrentPage = EPage::Menu;
    bool bUpgradeAvailable = true;
};
