#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GrandCityGlobalRoutingSubsystem.generated.h"

USTRUCT(BlueprintType)
struct GRANDCITYMOBILE_API FGrandCityRouteResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly)
    FString RegionId;

    UPROPERTY(BlueprintReadOnly)
    FString ServerId;

    UPROPERTY(BlueprintReadOnly)
    FString Endpoint;

    UPROPERTY(BlueprintReadOnly)
    FString Error;
};

declare_delegate_OneParam(FGrandCityRouteCallback, const FGrandCityRouteResult&);

UCLASS()
class GRANDCITYMOBILE_API UGrandCityGlobalRoutingSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    void FindBestServer(const TMap<FString, int32>& LatencyMsByRegion, FGrandCityRouteCallback Callback);

private:
    FString GetBaseUrl() const;
};
