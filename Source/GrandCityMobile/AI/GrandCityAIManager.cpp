// Grand City Mobile - server-safe AI orchestration layer.
#include "AI/GrandCityAIManager.h"

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

    // Transport is intentionally isolated here. The production implementation
    // should call the Grand City AI gateway, not OpenAI directly from the mobile client.
    // The gateway owns provider credentials, rate limits, moderation and validation.
    ActiveRequests.Add(RequestId);
    OnAIError.Broadcast(RequestId, TEXT("AI gateway transport is not configured yet."));
    ActiveRequests.Remove(RequestId);
}

bool UGrandCityAIManager::ValidateRequest(const FString& RequestId, const FString& UserPrompt) const
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
