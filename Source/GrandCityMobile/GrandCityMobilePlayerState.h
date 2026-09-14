#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "GrandCityMobilePlayerState.generated.h"

UCLASS()
class GRANDCITYMOBILE_API AGrandCityMobilePlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    AGrandCityMobilePlayerState();

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Grand City|Account")
    FString AccountId;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Grand City|Account")
    FString DisplayName;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Grand City|Account")
    FString RegionId;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Grand City|Account")
    bool bAuthenticated = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Grand City|Player")
    int32 CharacterLevel = 1;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Grand City|Player")
    int64 Cash = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Grand City|Player")
    int64 BankBalance = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Grand City|Player")
    int32 Reputation = 0;

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
