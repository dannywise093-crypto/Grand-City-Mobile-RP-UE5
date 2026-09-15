// Grand City Mobile - scalable mobile graphics profile definitions.
#pragma once

#include "CoreMinimal.h"
#include "GrandCityGraphicsProfile.generated.h"

UENUM(BlueprintType)
enum class EGrandCityGraphicsQuality : uint8
{
    Low,
    Medium,
    High,
    Ultra
};

USTRUCT(BlueprintType)
struct GRANDCITYMOBILE_API FGrandCityGraphicsProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|Graphics")
    EGrandCityGraphicsQuality Quality = EGrandCityGraphicsQuality::Medium;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|Graphics")
    int32 ResolutionScalePercent = 80;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|Graphics")
    int32 ShadowQuality = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|Graphics")
    int32 ViewDistanceQuality = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|Graphics")
    int32 EffectsQuality = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|Graphics")
    bool bEnableDynamicResolution = true;
};
