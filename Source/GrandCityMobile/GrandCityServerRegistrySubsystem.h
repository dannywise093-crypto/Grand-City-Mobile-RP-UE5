#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GrandCityServerRegistrySubsystem.generated.h"

UCLASS()
class GRANDCITYMOBILE_API UGrandCityServerRegistrySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="Grand City|Server Registry")
    void StartServerRegistration();

    UFUNCTION(BlueprintCallable, Category="Grand City|Server Registry")
    void BeginDraining();

    UFUNCTION(BlueprintPure, Category="Grand City|Server Registry")
    bool IsDraining() const { return bDraining; }

private:
    void SendHeartbeat();
    void SendHeartbeatRequest(bool bDrainingState);
    FString GetMasterRouterUrl() const;
    FString GetInternalApiKey() const;
    FString GetRegionId() const;
    FString GetServerId() const;
    FString GetServerEndpoint() const;
    int32 GetServerCapacity() const;

    FTimerHandle HeartbeatTimer;
    bool bRunning = false;
    bool bDraining = false;
};
