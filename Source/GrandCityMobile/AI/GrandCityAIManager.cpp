// Grand City Mobile - server-safe AI orchestration layer.
#include "AI/GrandCityAIManager.h"
#include "AI/GrandCityAIGatewayClient.h"

void UGrandCityAIManager::RequestNPCDialogue(const FString& NPCId, const FString& PlayerMessage, const FString& Context, const FString& RequestId)
{
    if (NPCId.IsEmpty())
    {
        OnAIError.Broadcast(RequestId, TEXT("NPC identity is required."));
        return;
    }

    const FString UserPrompt = FString::Printf(TEXT("NPC_ID=%s\nCONTEXT=%s\nPLAYER=%s"), *NPCId, *Context, *PlayerMessage);
    SubmitRequest(RequestId,
        TEXT("You are an NPC dialogue service for Grand City Mobile. Stay in character, keep responses concise, and never claim authority over account balances, inventory, permissions, purchases, or other authoritative game state. Return dialogue only."),
        UserPrompt);
}

void UGrandCityAIManager::RequestMissionSuggestion(const FString& PlayerId, const FString& DistrictId, const FString& Context, const FString& RequestId)
{
    if (PlayerId.IsEmpty() || DistrictId.IsEmpty())
    {
        OnAIError.Broadcast(RequestId, TEXT("Player and district identifiers are required."));
        return;
    }

    const FString UserPrompt = FString::Printf(TEXT("PLAYER_ID=%s\nDISTRICT_ID=%s\nCONTEXT=%s"), *PlayerId, *DistrictId, *Context);
    SubmitRequest(RequestId,
        TEXT("You are a mission suggestion service for Grand City Mobile. Suggest roleplay missions only. Never grant rewards, money, items, ranks, permissions, or other authoritative state. The game server must validate and apply any resulting mission."),
        UserPrompt);
}

void UGrandCityAIManager::CancelRequest(const FString& RequestId)
{
    ActiveRequests.Remove(RequestId);
}

void UGrandCityAIManager::SubmitRequest(const FString& RequestId, const FString& SystemPrompt, const FString& UserPrompt)
{
    if (!ValidateRequest(RequestId, UserPrompt))
    {
        return;
    }

    if (ActiveRequests.Contains(RequestId))
    {
        OnAIError.Broadcast(RequestId, TEXT("An AI request with this ID is already active."));
        return;
    }

    UGrandCityAIGatewayClient* Gateway = GetGameInstance()->GetSubsystem<UGrandCityAIGatewayClient>();
    if (!Gateway)
    {
        OnAIError.Broadcast(RequestId, TEXT("AI gateway client is unavailable."));
        return;
    }

    ActiveRequests.Add(RequestId);

    Gateway->SendChatRequest(
        RequestId,
        SystemPrompt,
        UserPrompt,
        FGrandCityAIGatewaySuccess::CreateWeakLambda(this,
            [this](const FString& CompletedRequestId, const FString& ResponseText)
            {
                ActiveRequests.Remove(CompletedRequestId);
                OnAIResponse.Broadcast(CompletedRequestId, ResponseText);
            }),
        FGrandCityAIGatewayFailure::CreateWeakLambda(this,
            [this](const FString& FailedRequestId, const FString& ErrorText)
            {
                ActiveRequests.Remove(FailedRequestId);
                OnAIError.Broadcast(FailedRequestId, ErrorText);
            }));
}

bool UGrandCityAIManager::ValidateRequest(const FString& RequestId, const FString& UserPrompt)
{
    if (RequestId.IsEmpty())
    {
        OnAIError.Broadcast(RequestId, TEXT("Request ID is required."));
        return false;
    }

    if (UserPrompt.Len() > 8000)
    {
        OnAIError.Broadcast(RequestId, TEXT("AI request context is too large."));
        return false;
    }

    return true;
}
