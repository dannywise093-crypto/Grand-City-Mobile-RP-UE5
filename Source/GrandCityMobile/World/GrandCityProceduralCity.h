#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrandCityProceduralCity.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EGrandCityDistrict : uint8
{
    Downtown,
    Residential,
    Commercial,
    Industrial,
    Services
};

UCLASS()
class GRANDCITYMOBILE_API AGrandCityProceduralCity : public AActor
{
    GENERATED_BODY()

public:
    AGrandCityProceduralCity();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    TObjectPtr<USceneComponent> CityRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    TObjectPtr<UStaticMeshComponent> Ground;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    TObjectPtr<UInstancedStaticMeshComponent> Roads;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    TObjectPtr<UInstancedStaticMeshComponent> Buildings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Layout", meta=(ClampMin="3", ClampMax="25"))
    int32 GridSize = 9;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Layout", meta=(ClampMin="500.0"))
    float BlockSize = 900.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Layout", meta=(ClampMin="100.0"))
    float RoadWidth = 220.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings", meta=(ClampMin="1", ClampMax="12"))
    int32 BuildingsPerBlock = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings", meta=(ClampMin="300.0"))
    float MinBuildingHeight = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings", meta=(ClampMin="500.0"))
    float MaxBuildingHeight = 2200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Generation")
    int32 Seed = 20260914;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Generation")
    bool bGenerateAtRuntime = true;

    void GenerateCity();
    EGrandCityDistrict GetDistrictForBlock(int32 X, int32 Y) const;
};
