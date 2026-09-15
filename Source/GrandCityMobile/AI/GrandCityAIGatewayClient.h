// Grand City Mobile - server-side AI gateway transport.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GrandCityAIGatewayClient.generated.h"

DECLARE_DELEGATE_TwoParams(FGrandCityAIGatewaySuccess, const FString&, const FString&);
DECLARE_DELEGATE_TwoParams(FGrandCityAIGatewayFailure, const FString&, const FString&);

UCLASS()
class GRANDCITYMOBILE_API UGrandCityAIGatewayClient : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    void SendChatRequest(const FString& RequestId, const FString& SystemPrompt, const FString& UserPrompt,
        FGrandCityAIGatewaySuccess OnSuccess, FGrandCityAIGatewayFailure OnFailure);

private:
    FString GetBaseUrl() const;
    FString GetGatewayKey() const;
};
