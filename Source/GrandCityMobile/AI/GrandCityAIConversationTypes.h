// Grand City Mobile - bounded AI conversation state.
#pragma once

#include "CoreMinimal.h"
#include "GrandCityAIConversationTypes.generated.h"

UENUM(BlueprintType)
enum class EGrandCityAIConversationRole : uint8
{
    Player,
    NPC
};

USTRUCT(BlueprintType)
struct GRANDCITYMOBILE_API FGrandCityAIConversationTurn
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Grand City|AI")
    EGrandCityAIConversationRole Role = EGrandCityAIConversationRole::Player;

    UPROPERTY(BlueprintReadOnly, Category="Grand City|AI")
    FString Text;
};
