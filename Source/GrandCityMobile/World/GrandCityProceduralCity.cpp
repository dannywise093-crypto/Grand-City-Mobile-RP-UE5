#include "GrandCityProceduralCity.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AGrandCityProceduralCity::AGrandCityProceduralCity()
{
    PrimaryActorTick.bCanEverTick = false;

    CityRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CityRoot"));
    SetRootComponent(CityRoot);

    Ground = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ground"));
    Ground->SetupAttachment(CityRoot);
    Ground->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Ground->SetCollisionResponseToAllChannels(ECR_Block);
    Ground->SetMobility(EComponentMobility::Static);

    Roads = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Roads"));
    Roads->SetupAttachment(CityRoot);
    Roads->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Roads->SetMobility(EComponentMobility::Static);

    Buildings = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Buildings"));
    Buildings->SetupAttachment(CityRoot);
    Buildings->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Buildings->SetMobility(EComponentMobility::Static);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        Ground->SetStaticMesh(CubeMesh.Object);
        Roads->SetStaticMesh(CubeMesh.Object);
        Buildings->SetStaticMesh(CubeMesh.Object);
    }
}

void AGrandCityProceduralCity::BeginPlay()
{
    Super::BeginPlay();

    if (bGenerateAtRuntime)
    {
        GenerateCity();
    }
}

EGrandCityDistrict AGrandCityProceduralCity::GetDistrictForBlock(int32 X, int32 Y) const
{
    const int32 Distance = FMath::Max(FMath::Abs(X), FMath::Abs(Y));

    if (Distance <= 1)
    {
        return EGrandCityDistrict::Downtown;
    }

    if (X < -2)
    {
        return EGrandCityDistrict::Industrial;
    }

    if (Y > 2)
    {
        return EGrandCityDistrict::Commercial;
    }

    if (X > 2)
    {
        return EGrandCityDistrict::Services;
    }

    return EGrandCityDistrict::Residential;
}

void AGrandCityProceduralCity::GenerateCity()
{
    if (!Ground || !Roads || !Buildings || !Roads->GetStaticMesh() || !Buildings->GetStaticMesh())
    {
        return;
    }

    Roads->ClearInstances();
    Buildings->ClearInstances();

    const float CellSize = BlockSize + RoadWidth;
    const float HalfWorld = (GridSize - 1) * CellSize * 0.5f;
    const float WorldExtent = GridSize * CellSize + RoadWidth;

    Ground->SetRelativeLocation(FVector(0.0f, 0.0f, -25.0f));
    Ground->SetRelativeScale3D(FVector(WorldExtent / 100.0f, WorldExtent / 100.0f, 0.5f));

    FRandomStream Random(Seed);

    for (int32 X = 0; X < GridSize; ++X)
    {
        const float XPos = X * CellSize - HalfWorld;
        Roads->AddInstance(FTransform(
            FRotator::ZeroRotator,
            FVector(XPos, 0.0f, 0.0f),
            FVector(RoadWidth / 100.0f, WorldExtent / 100.0f, 0.05f)));
    }

    for (int32 Y = 0; Y < GridSize; ++Y)
    {
        const float YPos = Y * CellSize - HalfWorld;
        Roads->AddInstance(FTransform(
            FRotator::ZeroRotator,
            FVector(0.0f, YPos, 2.0f),
            FVector(WorldExtent / 100.0f, RoadWidth / 100.0f, 0.05f)));
    }

    const float BuildingArea = BlockSize * 0.78f;

    for (int32 X = 0; X < GridSize - 1; ++X)
    {
        for (int32 Y = 0; Y < GridSize - 1; ++Y)
        {
            const int32 BlockX = X - (GridSize - 2) / 2;
            const int32 BlockY = Y - (GridSize - 2) / 2;
            const EGrandCityDistrict District = GetDistrictForBlock(BlockX, BlockY);

            for (int32 BuildingIndex = 0; BuildingIndex < BuildingsPerBlock; ++BuildingIndex)
            {
                const float OffsetX = Random.FRandRange(-BuildingArea * 0.35f, BuildingArea * 0.35f);
                const float OffsetY = Random.FRandRange(-BuildingArea * 0.35f, BuildingArea * 0.35f);

                float DistrictMin = MinBuildingHeight;
                float DistrictMax = MaxBuildingHeight;

                if (District == EGrandCityDistrict::Downtown)
                {
                    DistrictMin *= 1.8f;
                    DistrictMax *= 1.8f;
                }
                else if (District == EGrandCityDistrict::Industrial)
                {
                    DistrictMin *= 0.55f;
                    DistrictMax *= 0.8f;
                }
                else if (District == EGrandCityDistrict::Residential)
                {
                    DistrictMin *= 0.7f;
                    DistrictMax *= 1.15f;
                }

                const float Height = Random.FRandRange(DistrictMin, DistrictMax);
                const float Width = Random.FRandRange(110.0f, 190.0f);
                const float Depth = Random.FRandRange(110.0f, 190.0f);

                const float CenterX = X * CellSize - HalfWorld + CellSize * 0.5f;
                const float CenterY = Y * CellSize - HalfWorld + CellSize * 0.5f;

                Buildings->AddInstance(FTransform(
                    FRotator(0.0f, Random.FRandRange(0.0f, 359.0f), 0.0f),
                    FVector(CenterX + OffsetX, CenterY + OffsetY, Height * 0.5f),
                    FVector(Width / 100.0f, Depth / 100.0f, Height / 100.0f)));
            }
        }
    }
}
