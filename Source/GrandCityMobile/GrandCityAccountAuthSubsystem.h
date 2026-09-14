#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GrandCityAccountTypes.h"
#include "GrandCityAccountAuthSubsystem.generated.h"

delegate void FGrandCityAuthResult(bool bSuccess, const FGrandCityAccountIdentity& Identity);
delegate void FGrandCitySessionResult(bool bSuccess, const FString& TransferToken);
delegate void FGrandCityTicketConsumeResult(bool bSuccess, const FString& AccountId);

UCLASS()
class GRANDCITYMOBILE_API UGrandCityAccountAuthSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    void VerifyToken(const FString& AuthToken, FGrandCityAuthResult Callback);
    void ConsumeConnectionTicket(const FString& Ticket, const FString& ServerId, FGrandCityTicketConsumeResult Callback);
    void ClaimSession(const FString& AccountId, const FString& ServerId, const FString& TransferToken, FGrandCitySessionResult Callback);
    void ReleaseSession(const FString& AccountId, const FString& ServerId, TFunction<void(bool)> Callback);

    bool IsAuthenticated() const { return Identity.bAuthenticated; }
    const FGrandCityAccountIdentity& GetIdentity() const { return Identity; }

private:
    FString GetBaseUrl() const;
    FString GetInternalApiKey() const;
    void SetIdentity(const FGrandCityAccountIdentity& InIdentity);

    FGrandCityAccountIdentity Identity;
};
