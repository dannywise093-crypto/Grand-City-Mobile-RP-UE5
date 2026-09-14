#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GrandCityAccountTypes.h"
#include "GrandCityMobileAccountClientSubsystem.generated.h"

delegate void FGrandCityAccountClientResult(bool bSuccess, const FGrandCityAccountIdentity& Identity, const FString& AuthToken, const FString& ErrorCode);
delegate void FGrandCityConnectionTicketResult(bool bSuccess, const FString& Ticket, const FString& ErrorCode);

UCLASS()
class GRANDCITYMOBILE_API UGrandCityMobileAccountClientSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    void RegisterAccount(const FString& DisplayName, const FString& Password, FGrandCityAccountClientResult Callback);
    void LoginAccount(const FString& DisplayName, const FString& Password, FGrandCityAccountClientResult Callback);
    void RequestConnectionTicket(const FString& TargetServerId, FGrandCityConnectionTicketResult Callback);

    const FGrandCityAccountIdentity& GetIdentity() const { return Identity; }
    const FString& GetAuthToken() const { return AuthToken; }
    bool IsAuthenticated() const { return Identity.bAuthenticated && !AuthToken.IsEmpty(); }
    void ClearSession();

private:
    FString GetBaseUrl() const;
    void SendCredentialsRequest(const TCHAR* Endpoint, const FString& DisplayName, const FString& Password, FGrandCityAccountClientResult Callback);

    FGrandCityAccountIdentity Identity;
    FString AuthToken;
};
