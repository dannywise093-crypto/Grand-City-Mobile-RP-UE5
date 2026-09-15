// Grand City Mobile - server-side NPC personality and conversation brain.
#include "AI/GrandCityNPCBrain.h"
#include "AI/GrandCityAIManager.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Misc/Guid.h"

UGrandCityNPCBrain::UGrandCityNPCBrain()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

FString UGrandCityNPCBrain::SanitizeText(const FString& Text, int32 MaxChars) const
{
    FString Clean = Text;
    Clean.ReplaceInline(TEXT("\r"), TEXT(" "));
    Clean.ReplaceInline(TEXT("\n"), TEXT(" "));
    Clean.TrimStartAndEndInline();
    if (Clean.Len() > MaxChars)
    {
        Clean.LeftInline(MaxChars);
    }
    return Clean;
}

FString UGrandCityNPCBrain::MakeRequestId(const FString& PlayerId) const
{
    return FString::Printf(TEXT("npc-%s-%s"), *SanitizeText(NPCId, 64), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

FString UGrandCityNPCBrain::BuildContext(const FString& PlayerId) const
{
    const FConversationState* State = Conversations.Find(PlayerId);
    FString Context = FString::Printf(
        TEXT("NPC_ID=%s\nNAME=%s\nPERSONALITY=%s\nOCCUPATION=%s\nDISTRICT=%s\nPLAYER_ID=%s\n"),
        *SanitizeText(NPCId, 64),
        *SanitizeText(DisplayName, 80),
        *SanitizeText(Personality, 600),
        *SanitizeText(Occupation, 120),
        *SanitizeText(DistrictId, 80),
        *SanitizeText(PlayerId, 80));

    if (!State)
    {
        return Context + TEXT("CONVERSATION=none");
    }

    Context += TEXT("CONVERSATION=\n");
    for (const FGrandCityAIConversationTurn& Turn : State->Turns)
    {
        const TCHAR* Role = Turn.Role == EGrandCityAIConversationRole::NPC ? TEXT("NPC") : TEXT("PLAYER");
        Context += FString::Printf(TEXT("%s: %s\n"), Role, *SanitizeText(Turn.Text, 700));
    }
    return Context;
}

void UGrandCityNPCBrain::SpeakToPlayer(const FString& PlayerId, const FString& PlayerMessage)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        OnDialogueError.Broadcast(TEXT(""), TEXT("NPC AI is server-authoritative."));
        return;
    }

    const FString SafePlayerId = SanitizeText(PlayerId, 80);
    const FString SafeMessage = SanitizeText(PlayerMessage, 1200);
    if (SafePlayerId.IsEmpty() || SafeMessage.IsEmpty())
    {
        OnDialogueError.Broadcast(TEXT(""), TEXT("Player ID and message are required."));
        return;
    }

    if (PendingRequests.Contains(SafePlayerId))
    {
        OnDialogueError.Broadcast(PendingRequests[SafePlayerId], TEXT("NPC is already responding to this player."));
        return;
    }

    FConversationState& State = Conversations.FindOrAdd(SafePlayerId);
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if ((Now - State.LastRequestSeconds) < CooldownSeconds)
    {
        OnDialogueError.Broadcast(TEXT(""), TEXT("NPC dialogue is on cooldown."));
        return;
    }

    const FString RequestId = MakeRequestId(SafePlayerId);
    State.LastRequestSeconds = Now;

    FGrandCityAIConversationTurn PlayerTurn;
    PlayerTurn.Role = EGrandCityAIConversationRole::Player;
    PlayerTurn.Text = SafeMessage;
    State.Turns.Add(PlayerTurn);

    const int32 MaxTurns = FMath::Max(1, MaxConversationTurns);
    while (State.Turns.Num() > MaxTurns)
    {
        State.Turns.RemoveAt(0);
    }

    UGrandCityAIManager* AIManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGrandCityAIManager>() : nullptr;
    if (!AIManager)
    {
        OnDialogueError.Broadcast(RequestId, TEXT("AI manager is unavailable."));
        return;
    }

    PendingRequests.Add(SafePlayerId, RequestId);
    const FString Context = BuildContext(SafePlayerId);
    AIManager->OnAIResponse.AddUniqueDynamic(this, &UGrandCityNPCBrain::HandleAIResponse);
    AIManager->OnAIError.AddUniqueDynamic(this, &UGrandCityNPCBrain::HandleAIError);
    AIManager->RequestNPCDialogue(NPCId, SafeMessage, Context, RequestId);
}

void UGrandCityNPCBrain::ClearConversation(const FString& PlayerId)
{
    Conversations.Remove(SanitizeText(PlayerId, 80));
}

void UGrandCityNPCBrain::HandleAIResponse(const FString& RequestId, const FString& ResponseText)
{
    FString PlayerId;
    for (const TPair<FString, FString>& Pair : PendingRequests)
    {
        if (Pair.Value == RequestId)
        {
            PlayerId = Pair.Key;
            break;
        }
    }

    if (PlayerId.IsEmpty())
    {
        return;
    }

    PendingRequests.Remove(PlayerId);
    const FString SafeResponse = SanitizeText(ResponseText, 1200);
    if (SafeResponse.IsEmpty())
    {
        OnDialogueError.Broadcast(RequestId, TEXT("NPC received an empty AI response."));
        return;
    }

    FConversationState& State = Conversations.FindOrAdd(PlayerId);
    FGrandCityAIConversationTurn NPCTurn;
    NPCTurn.Role = EGrandCityAIConversationRole::NPC;
    NPCTurn.Text = SafeResponse;
    State.Turns.Add(NPCTurn);
    while (State.Turns.Num() > FMath::Max(1, MaxConversationTurns))
    {
        State.Turns.RemoveAt(0);
    }

    OnDialogueReady.Broadcast(RequestId, SafeResponse);
}

void UGrandCityNPCBrain::HandleAIError(const FString& RequestId, const FString& ErrorText)
{
    for (auto It = PendingRequests.CreateIterator(); It; ++It)
    {
        if (It.Value() == RequestId)
        {
            It.RemoveCurrent();
            break;
        }
    }
    OnDialogueError.Broadcast(RequestId, ErrorText);
}
