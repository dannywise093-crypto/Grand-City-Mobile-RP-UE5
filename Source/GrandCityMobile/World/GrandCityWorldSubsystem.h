#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GrandCityWorldSubsystem.generated.h"

UCLASS()
class GRANDCITYMOBILE_API UGrandCityWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

    UFUNCTION(BlueprintPure, Category="Grand City|World")
    bool IsCityPrototypeSpawned() const { return bCityPrototypeSpawned; }

private:
    bool bCityPrototypeSpawned = false;
};
