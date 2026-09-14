#pragma once

#include "CoreMinimal.h"
#include "GrandCityPlayerProfileTypes.generated.h"

USTRUCT(BlueprintType)
struct GRANDCITYMOBILE_API FGrandCityPlayerProfile
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Profile")
    int32 SchemaVersion = 1;

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Profile")
    FString AccountId;

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Profile")
    FString CharacterId;

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Profile")
    FString CharacterName;

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Profile")
    FString RegionId;

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Profile")
    int32 CharacterLevel = 1;

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Profile")
    int64 Cash = 0;

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Profile")
    int64 BankBalance = 0;

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Profile")
    int32 Reputation = 0;

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Profile")
    int64 TotalPlayTimeSeconds = 0;

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Profile")
    int64 LastSaveUnixSeconds = 0;
};
