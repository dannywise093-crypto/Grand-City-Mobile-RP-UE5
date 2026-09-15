// Grand City Mobile - server-side NPC personality and conversation brain.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrandCityAIConversationTypes.h"
#include "GrandCityNPCBrain.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGrandCityNPCDialogueReady, const FString&, RequestId, const FString&, ResponseText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGrandCityNPCDialogueError, const FString&, RequestId, const FString&, ErrorText);

UCLASS(ClassGroup=(GrandCity), meta=(BlueprintSpawnableComponent))
class GRANDCITYMOBILE_API UGrandCityNPCBrain : public UActorComponent
{
    GENERATED_BODY()

public:
    UGrandCityNPCBrain();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|AI|NPC")
    FString NPCId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|AI|NPC")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|AI|NPC")
    FString Personality;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|AI|NPC")
    FString Occupation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|AI|NPC")
    FString DistrictId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|AI|NPC", meta=(ClampMin="1", ClampMax="20"))
    int32 MaxConversationTurns = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|AI|NPC", meta=(ClampMin="0.0", ClampMax="60.0"))
    float CooldownSeconds = 3.0f;

    UFUNCTION(BlueprintCallable, Category="Grand City|AI|NPC")
    void SpeakToPlayer(const FString& PlayerId, const FString& PlayerMessage);

    UFUNCTION(BlueprintCallable, Category="Grand City|AI|NPC")
    void ClearConversation(const FString& PlayerId);

    UPROPERTY(BlueprintAssignable, Category="Grand City|AI|NPC")
    FGrandCityNPCDialogueReady OnDialogueReady;

    UPROPERTY(BlueprintAssignable, Category="Grand City|AI|NPC")
    FGrandCityNPCDialogueError OnDialogueError;

private:
    struct FConversationState
    {
        TArray<FGrandCityAIConversationTurn> Turns;
        double LastRequestSeconds = -DBL_MAX;
    };

    FString BuildContext(const FString& PlayerId) const;
    FString SanitizeText(const FString& Text, int32 MaxChars) const;
    FString MakeRequestId(const FString& PlayerId) const;
    void HandleAIResponse(const FString& RequestId, const FString& ResponseText);
    void HandleAIError(const FString& RequestId, const FString& ErrorText);

    TMap<FString, FConversationState> Conversations;
    TMap<FString, FString> PendingRequests;
};
