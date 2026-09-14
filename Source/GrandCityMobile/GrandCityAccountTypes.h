#pragma once

#include "CoreMinimal.h"
#include "GrandCityAccountTypes.generated.h"

USTRUCT(BlueprintType)
struct GRANDCITYMOBILE_API FGrandCityAccountIdentity
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Account")
    FString AccountId;

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Account")
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Account")
    FString RegionId;

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Account")
    bool bAuthenticated = false;
};
