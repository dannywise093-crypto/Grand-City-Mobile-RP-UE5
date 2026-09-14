#include "GrandCityMobileAuthWidget.h"
#include "GrandCityMobileAccountClientSubsystem.h"
#include "GrandCityMobilePlayerController.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "Components/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "Internationalization/Text.h"

void UGrandCityMobileAuthWidget::NativeConstruct(){ Super::NativeConstruct(); BuildInterface(); }
void UGrandCityMobileAuthWidget::BuildInterface(){
    if(!WidgetTree)return;
    UBorder* Background=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    UVerticalBox* Panel=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Background->SetPadding(FMargin(40.0f)); Background->AddChild(Panel); WidgetTree->RootWidget=Background;
    TitleText=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); TitleText->SetText(FText::FromString(TEXT("GRAND CITY MOBILE"))); Panel->AddChildToVerticalBox(TitleText);
    UTextBlock* Subtitle=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); Subtitle->SetText(FText::FromString(TEXT("Worldwide Account Login"))); Panel->AddChildToVerticalBox(Subtitle);
    DisplayNameInput=WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass()); DisplayNameInput->SetHintText(FText::FromString(TEXT("Display name"))); Panel->AddChildToVerticalBox(DisplayNameInput);
    PasswordInput=WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass()); PasswordInput->SetHintText(FText::FromString(TEXT("Password"))); PasswordInput->SetIsPassword(true); Panel->AddChildToVerticalBox(PasswordInput);
    ConfirmPasswordInput=WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass()); ConfirmPasswordInput->SetHintText(FText::FromString(TEXT("Confirm password"))); ConfirmPasswordInput->SetIsPassword(true); ConfirmPasswordInput->SetVisibility(ESlateVisibility::Collapsed); Panel->AddChildToVerticalBox(ConfirmPasswordInput);
    PrimaryButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass()); UTextBlock* PrimaryLabel=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); PrimaryLabel->SetText(FText::FromString(TEXT("LOGIN"))); PrimaryButton->AddChild(PrimaryLabel); Panel->AddChildToVerticalBox(PrimaryButton); PrimaryButton->OnClicked.AddDynamic(this,&UGrandCityMobileAuthWidget::HandleLoginClicked);
    ModeButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass()); ModeText=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); ModeText->SetText(FText::FromString(TEXT("Create a new account"))); ModeButton->AddChild(ModeText); Panel->AddChildToVerticalBox(ModeButton); ModeButton->OnClicked.AddDynamic(this,&UGrandCityMobileAuthWidget::HandleModeClicked);
    StatusText=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); Panel->AddChildToVerticalBox(StatusText);
}
void UGrandCityMobileAuthWidget::HandleLoginClicked(){
    if(bRegisterMode){ HandleRegisterClicked(); return; }
    if(bBusy||!DisplayNameInput||!PasswordInput)return;
    const FString DisplayName=DisplayNameInput->GetText().ToString().TrimStartAndEnd(); const FString Password=PasswordInput->GetText().ToString();
    if(DisplayName.IsEmpty()||Password.IsEmpty()){SetStatus(TEXT("Enter your display name and password."),true);return;}
    UGameInstance* GI=GetGameInstance(); UGrandCityMobileAccountClientSubsystem* AccountClient=GI?GI->GetSubsystem<UGrandCityMobileAccountClientSubsystem>():nullptr;
    if(!AccountClient){SetStatus(TEXT("Account service unavailable."),true);return;} SetBusy(true); SetStatus(TEXT("Signing in..."));
    AccountClient->LoginAccount(DisplayName,Password,FGrandCityAccountClientResult::CreateUObject(this,&UGrandCityMobileAuthWidget::CompleteAuthentication));
}
void UGrandCityMobileAuthWidget::HandleRegisterClicked(){
    if(bBusy||!DisplayNameInput||!PasswordInput||!ConfirmPasswordInput)return;
    const FString DisplayName=DisplayNameInput->GetText().ToString().TrimStartAndEnd(); const FString Password=PasswordInput->GetText().ToString(); const FString Confirm=ConfirmPasswordInput->GetText().ToString();
    if(DisplayName.IsEmpty()||Password.IsEmpty()||Confirm.IsEmpty()){SetStatus(TEXT("Complete all account fields."),true);return;} if(Password!=Confirm){SetStatus(TEXT("Passwords do not match."),true);return;}
    UGameInstance* GI=GetGameInstance(); UGrandCityMobileAccountClientSubsystem* AccountClient=GI?GI->GetSubsystem<UGrandCityMobileAccountClientSubsystem>():nullptr;
    if(!AccountClient){SetStatus(TEXT("Account service unavailable."),true);return;} SetBusy(true); SetStatus(TEXT("Creating account..."));
    AccountClient->RegisterAccount(DisplayName,Password,FGrandCityAccountClientResult::CreateUObject(this,&UGrandCityMobileAuthWidget::CompleteAuthentication));
}
void UGrandCityMobileAuthWidget::HandleModeClicked(){
    if(bBusy)return; bRegisterMode=!bRegisterMode;
    if(TitleText)TitleText->SetText(FText::FromString(bRegisterMode?TEXT("CREATE ACCOUNT"):TEXT("GRAND CITY MOBILE")));
    if(ConfirmPasswordInput)ConfirmPasswordInput->SetVisibility(bRegisterMode?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(PrimaryButton)if(UTextBlock* Label=Cast<UTextBlock>(PrimaryButton->GetChildAt(0)))Label->SetText(FText::FromString(bRegisterMode?TEXT("REGISTER"):TEXT("LOGIN")));
    if(ModeText)ModeText->SetText(FText::FromString(bRegisterMode?TEXT("Already have an account? Login"):TEXT("Create a new account"))); SetStatus(TEXT(""));
}
void UGrandCityMobileAuthWidget::SetStatus(const FString& Message,bool bError){if(StatusText)StatusText->SetText(FText::FromString(Message));}
void UGrandCityMobileAuthWidget::SetBusy(bool bInBusy){bBusy=bInBusy;if(PrimaryButton)PrimaryButton->SetIsEnabled(!bBusy);if(ModeButton)ModeButton->SetIsEnabled(!bBusy);}
void UGrandCityMobileAuthWidget::CompleteAuthentication(bool bSuccess,const FGrandCityAccountIdentity& Identity,const FString& AuthToken,const FString& ErrorCode){
    SetBusy(false); if(!bSuccess){SetStatus(FString::Printf(TEXT("Authentication failed: %s"),*ErrorCode),true);return;}
    AGrandCityMobilePlayerController* PC=GetOwningPlayer<AGrandCityMobilePlayerController>(); if(!PC){SetStatus(TEXT("Player controller unavailable."),true);return;}
    PC->SetAuthCredentials(AuthToken,FString()); SetStatus(TEXT("Authenticated. Finding the best worldwide server...")); PC->TravelToBestWorldwideServer();
}
