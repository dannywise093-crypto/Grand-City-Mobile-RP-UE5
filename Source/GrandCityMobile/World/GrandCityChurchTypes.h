#pragma once

#include "CoreMinimal.h"
#include "GrandCityChurchTypes.generated.h"

UENUM(BlueprintType)
enum class EGrandCityChurchType : uint8
{
    Community,
    Modern,
    HillsideChapel,
    WaterfrontChapel,
    GrandCathedral
};

USTRUCT(BlueprintType)
struct GRANDCITYMOBILE_API FGrandCityChurchDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|Church")
    EGrandCityChurchType Type = EGrandCityChurchType::Community;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|Church")
    float FootprintScale = 2.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|Church")
    float HeightScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|Church")
    bool bHasBellTower = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|Church")
    bool bHasParking = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|Church")
    bool bHasLandscapedGrounds = true;
};
