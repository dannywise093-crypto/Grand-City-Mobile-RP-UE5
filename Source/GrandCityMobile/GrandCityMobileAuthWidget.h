#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GrandCityMobileAuthWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UVerticalBox;

UCLASS()
class GRANDCITYMOBILE_API UGrandCityMobileAuthWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

protected:
    UFUNCTION()
    void HandleLoginClicked();

    UFUNCTION()
    void HandleRegisterClicked();

    UFUNCTION()
    void HandleModeClicked();

private:
    void BuildInterface();
    void SetStatus(const FString& Message, bool bError = false);
    void SetBusy(bool bBusy);
    void CompleteAuthentication(bool bSuccess, const struct FGrandCityAccountIdentity& Identity, const FString& AuthToken, const FString& ErrorCode);

    UPROPERTY()
    TObjectPtr<UEditableTextBox> DisplayNameInput;

    UPROPERTY()
    TObjectPtr<UEditableTextBox> PasswordInput;

    UPROPERTY()
    TObjectPtr<UEditableTextBox> ConfirmPasswordInput;

    UPROPERTY()
    TObjectPtr<UButton> PrimaryButton;

    UPROPERTY()
    TObjectPtr<UButton> ModeButton;

    UPROPERTY()
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY()
    TObjectPtr<UTextBlock> StatusText;

    UPROPERTY()
    TObjectPtr<UTextBlock> ModeText;

    bool bRegisterMode = false;
    bool bBusy = false;
};
