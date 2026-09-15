// Grand City Mobile - lightweight mobile NPC interaction bridge.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrandCityNPCInteractionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGrandCityNPCInteractionChanged, bool, bCanInteract);

UCLASS(ClassGroup=(GrandCity), meta=(BlueprintSpawnableComponent))
class GRANDCITYMOBILE_API UGrandCityNPCInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGrandCityNPCInteractionComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grand City|NPC")
    float InteractionDistance = 350.0f;

    UFUNCTION(BlueprintCallable, Category="Grand City|NPC")
    bool CanPlayerInteract(AActor* PlayerActor) const;

    UFUNCTION(BlueprintCallable, Category="Grand City|NPC")
    void SetInteractionEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category="Grand City|NPC")
    bool IsInteractionEnabled() const { return bInteractionEnabled; }

    UPROPERTY(BlueprintAssignable, Category="Grand City|NPC")
    FGrandCityNPCInteractionChanged OnInteractionChanged;

private:
    bool bInteractionEnabled = true;
};
