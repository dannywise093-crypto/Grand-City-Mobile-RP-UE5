// Grand City Mobile - server-safe AI orchestration layer.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GrandCityAIManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGrandCityAIResponse, const FString&, RequestId, const FString&, ResponseText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGrandCityAIError, const FString&, RequestId, const FString&, ErrorText);

UCLASS()
class GRANDCITYMOBILE_API UGrandCityAIManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Grand City|AI")
    void RequestNPCDialogue(const FString& NPCId, const FString& PlayerMessage, const FString& Context, const FString& RequestId);

    UFUNCTION(BlueprintCallable, Category="Grand City|AI")
    void RequestMissionSuggestion(const FString& PlayerId, const FString& DistrictId, const FString& Context, const FString& RequestId);

    UFUNCTION(BlueprintCallable, Category="Grand City|AI")
    void CancelRequest(const FString& RequestId);

    UPROPERTY(BlueprintAssignable, Category="Grand City|AI")
    FGrandCityAIResponse OnAIResponse;

    UPROPERTY(BlueprintAssignable, Category="Grand City|AI")
    FGrandCityAIError OnAIError;

private:
    void SubmitRequest(const FString& RequestId, const FString& SystemPrompt, const FString& UserPrompt);
    bool ValidateRequest(const FString& RequestId, const FString& UserPrompt) const;

    TSet<FString> ActiveRequests;
};
