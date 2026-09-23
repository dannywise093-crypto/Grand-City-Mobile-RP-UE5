#include "GrandCityProceduralCity.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/DateTime.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace GrandCityGeometry
{
    constexpr float GroundThickness = 50.0f;
    constexpr float RoadThickness = 8.0f;
    constexpr float BlockSurfaceHeight = 10.0f;
    constexpr float SidewalkHeight = 22.0f;
    constexpr float CurbHeight = 28.0f;
    constexpr float CurbDepth = 18.0f;
    constexpr float MarkingHeight = 2.5f;
    constexpr float CrosswalkWidth = 340.0f;
    constexpr int32 CrosswalkStripeCount = 8;

    /** Edge-to-edge span of a hexagon divided by its circumradius. */
    constexpr float HexagonFlatToFlat = 1.7320508f;

    /** Each hexagon slab is shortened a hair so their caps never z-fight. */
    constexpr float HexagonSlabStep = 0.8f;

    constexpr uint32 BuildingSalt = 0x47A1B21Du;
    constexpr uint32 FurnitureSalt = 0x15F0C9A7u;

    /** Puts a min/max pair back in order after a designer edits it. */
    void OrderRange(FVector2D& Range, float Minimum, float Maximum)
    {
        Range.X = FMath::Clamp(static_cast<float>(Range.X), Minimum, Maximum);
        Range.Y = FMath::Clamp(static_cast<float>(Range.Y), static_cast<float>(Range.X), Maximum);
    }

    /** Picks an index from relative weights; returns the last entry as fallback. */
    int32 PickWeighted(FRandomStream& Random, const float* Weights, int32 Count)
    {
        float Total = 0.0f;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            Total += FMath::Max(0.0f, Weights[Index]);
        }

        if (Total <= KINDA_SMALL_NUMBER)
        {
            return Count - 1;
        }

        float Roll = Random.FRandRange(0.0f, Total);
        for (int32 Index = 0; Index < Count; ++Index)
        {
            Roll -= FMath::Max(0.0f, Weights[Index]);
            if (Roll <= 0.0f)
            {
                return Index;
            }
        }
        return Count - 1;
    }
}

AGrandCityProceduralCity::AGrandCityProceduralCity()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    SetReplicateMovement(false);

    CityRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CityRoot"));
    CityRoot->SetMobility(EComponentMobility::Static);
    SetRootComponent(CityRoot);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlatMaterialFinder(
        TEXT("/Game/LevelPrototyping/Materials/M_FlatCol.M_FlatCol"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMaterialFinder(
        TEXT("/Game/LevelPrototyping/Interactable/JumpPad/Assets/Materials/M_SimpleGlow.M_SimpleGlow"));

    CubeMesh = CubeFinder.Object;
    CylinderMesh = CylinderFinder.Object;
    SphereMesh = SphereFinder.Object;
    FlatColorMaterial = FlatMaterialFinder.Object;
    GlowMaterial = GlowMaterialFinder.Object;

    Ground = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ground"));
    Ground->SetupAttachment(CityRoot);
    Ground->SetStaticMesh(CubeMesh);
    Ground->SetMobility(EComponentMobility::Static);
    Ground->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Ground->SetCollisionResponseToAllChannels(ECR_Block);
    Ground->SetGenerateOverlapEvents(false);
    Ground->SetCanEverAffectNavigation(true);
    Ground->SetCastShadow(false);
    if (FlatColorMaterial)
    {
        Ground->SetMaterial(0, FlatColorMaterial);
    }

    Roads = CreateInstancedComponent(TEXT("Roads"), CubeMesh, ECollisionEnabled::QueryAndPhysics, false, false);
    BlockSurfaces = CreateInstancedComponent(TEXT("BlockSurfaces"), CubeMesh, ECollisionEnabled::QueryAndPhysics, false, false);
    ParkSurfaces = CreateInstancedComponent(TEXT("ParkSurfaces"), CubeMesh, ECollisionEnabled::QueryAndPhysics, false, false);
    Sidewalks = CreateInstancedComponent(TEXT("Sidewalks"), CubeMesh, ECollisionEnabled::QueryAndPhysics, false, false);
    Curbs = CreateInstancedComponent(TEXT("Curbs"), CubeMesh, ECollisionEnabled::QueryAndPhysics, false, false);
    RoadMarkings = CreateInstancedComponent(TEXT("RoadMarkings"), CubeMesh, ECollisionEnabled::NoCollision, false, false);
    Crosswalks = CreateInstancedComponent(TEXT("Crosswalks"), CubeMesh, ECollisionEnabled::NoCollision, false, false);

    BuildingStyleA = CreateInstancedComponent(TEXT("BuildingStyleA"), CubeMesh, ECollisionEnabled::QueryAndPhysics, true);
    BuildingStyleB = CreateInstancedComponent(TEXT("BuildingStyleB"), CubeMesh, ECollisionEnabled::QueryAndPhysics, true);
    BuildingStyleC = CreateInstancedComponent(TEXT("BuildingStyleC"), CubeMesh, ECollisionEnabled::QueryAndPhysics, true);
    BuildingStyleD = CreateInstancedComponent(TEXT("BuildingStyleD"), CubeMesh, ECollisionEnabled::QueryAndPhysics, true);
    BuildingRoofs = CreateInstancedComponent(TEXT("BuildingRoofs"), CubeMesh, ECollisionEnabled::NoCollision, false);
    BuildingWindows = CreateInstancedComponent(TEXT("BuildingWindows"), CubeMesh, ECollisionEnabled::NoCollision, false, false);

    StreetLightPoles = CreateInstancedComponent(TEXT("StreetLightPoles"), CylinderMesh, ECollisionEnabled::QueryAndPhysics, true);
    StreetLightArms = CreateInstancedComponent(TEXT("StreetLightArms"), CubeMesh, ECollisionEnabled::NoCollision, false);
    StreetLightHeads = CreateInstancedComponent(TEXT("StreetLightHeads"), CubeMesh, ECollisionEnabled::NoCollision, false, false);

    TrafficLightPoles = CreateInstancedComponent(TEXT("TrafficLightPoles"), CylinderMesh, ECollisionEnabled::QueryAndPhysics, true);
    TrafficLightHousings = CreateInstancedComponent(TEXT("TrafficLightHousings"), CubeMesh, ECollisionEnabled::NoCollision, false);
    TrafficSignalsRed = CreateInstancedComponent(TEXT("TrafficSignalsRed"), SphereMesh, ECollisionEnabled::NoCollision, false, false);
    TrafficSignalsAmber = CreateInstancedComponent(TEXT("TrafficSignalsAmber"), SphereMesh, ECollisionEnabled::NoCollision, false, false);
    TrafficSignalsGreen = CreateInstancedComponent(TEXT("TrafficSignalsGreen"), SphereMesh, ECollisionEnabled::NoCollision, false, false);

    TreeTrunks = CreateInstancedComponent(TEXT("TreeTrunks"), CylinderMesh, ECollisionEnabled::QueryAndPhysics, true);
    TreeCanopiesA = CreateInstancedComponent(TEXT("TreeCanopiesA"), SphereMesh, ECollisionEnabled::NoCollision, false);
    TreeCanopiesB = CreateInstancedComponent(TEXT("TreeCanopiesB"), SphereMesh, ECollisionEnabled::NoCollision, false);
}

UHierarchicalInstancedStaticMeshComponent* AGrandCityProceduralCity::CreateInstancedComponent(
    const FName& Name,
    UStaticMesh* Mesh,
    ECollisionEnabled::Type Collision,
    bool bAffectsNavigation,
    bool bCastsShadow)
{
    UHierarchicalInstancedStaticMeshComponent* Component =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(Name);
    Component->SetupAttachment(CityRoot);
    Component->SetStaticMesh(Mesh);
    Component->SetMobility(EComponentMobility::Static);
    Component->SetCollisionEnabled(Collision);
    Component->SetCollisionResponseToAllChannels(ECR_Block);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(bAffectsNavigation);
    Component->CastShadow = bCastsShadow;
    Component->bCastDynamicShadow = bCastsShadow;
    Component->SetCullDistances(0, 80000);

    if (FlatColorMaterial)
    {
        Component->SetMaterial(0, FlatColorMaterial);
    }

    return Component;
}

void AGrandCityProceduralCity::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // Materials are transient visual state. Crucially, construction never
    // clears or regenerates the serialized HISM layout.
    ApplyVisualMaterials();
}

void AGrandCityProceduralCity::PostActorCreated()
{
    Super::PostActorCreated();

    // Dropping the actor into a level bakes it once so it is never empty.
    // Loading a saved level does not route through PostActorCreated, so an
    // already baked city keeps exactly the layout that was serialized.
    if (!bHasBakedLayout && CanEditBakedLayout())
    {
        RegenerateCity();
    }
}

#if WITH_EDITOR
void AGrandCityProceduralCity::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    if (PropertyName == GET_MEMBER_NAME_CHECKED(AGrandCityProceduralCity, bRequestRegenerate) && bRequestRegenerate)
    {
        // Consume the request first so the bake cannot re-enter this handler.
        bRequestRegenerate = false;
        RegenerateCity();
    }
}
#endif

void AGrandCityProceduralCity::BeginPlay()
{
    Super::BeginPlay();

    ApplyVisualMaterials();

    const bool bHasSerializedGeometry = Roads && Roads->GetInstanceCount() > 0;
    if (!bHasBakedLayout || !bHasSerializedGeometry)
    {
        UE_LOG(LogTemp, Error,
            TEXT("Grand City '%s' has no baked layout. Open the map in the editor and press Regenerate City. Runtime generation is intentionally disabled."),
            *GetName());
        return;
    }

    const int32 CurrentConfigHash = ComputeConfigHash();
    if (CurrentConfigHash != BakedConfigHash || BakedGeneratorVersion != GeneratorVersion)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Grand City '%s' is using baked seed %d and will not refresh during Play. Settings/code changed since the bake; press Regenerate City in the editor when ready."),
            *GetName(), BakedSeed);
    }

    UE_LOG(LogTemp, Display,
        TEXT("Loaded baked Grand City '%s': seed=%d, buildings=%d, roads=%d, sidewalks=%d, crossings=%d, lights=%d/%d, trees=%d. No runtime regeneration performed."),
        *GetName(),
        BakedSeed,
        GenerationStats.LogicalBuildings,
        GenerationStats.RoadSegments,
        GenerationStats.SidewalkSections,
        GenerationStats.CrosswalkStripes,
        GenerationStats.StreetLights,
        GenerationStats.TrafficLights,
        GenerationStats.Trees);
}

void AGrandCityProceduralCity::SanitizeSettings()
{
    GridSize = FMath::Clamp(GridSize, 2, 12);
    BlockSize = FMath::Clamp(BlockSize, 1800.0f, 8000.0f);
    RoadWidth = FMath::Clamp(RoadWidth, 700.0f, 2400.0f);
    SidewalkWidth = FMath::Clamp(SidewalkWidth, 160.0f, FMath::Min(600.0f, BlockSize * 0.2f));
    BuildingSetback = FMath::Clamp(BuildingSetback, 40.0f, 500.0f);
    MinBuildingsPerBlock = FMath::Clamp(MinBuildingsPerBlock, 1, 4);
    MaxBuildingsPerBlock = FMath::Clamp(MaxBuildingsPerBlock, FMath::Max(2, MinBuildingsPerBlock), 7);

    SmallBuildingWeight = FMath::Max(0.0f, SmallBuildingWeight);
    MediumBuildingWeight = FMath::Max(0.0f, MediumBuildingWeight);
    LargeBuildingWeight = FMath::Max(0.0f, LargeBuildingWeight);
    BoxShapeWeight = FMath::Max(0.0f, BoxShapeWeight);
    RectangleShapeWeight = FMath::Max(0.0f, RectangleShapeWeight);
    HexagonShapeWeight = FMath::Max(0.0f, HexagonShapeWeight);
    ShortHeightWeight = FMath::Max(0.0f, ShortHeightWeight);
    MidHeightWeight = FMath::Max(0.0f, MidHeightWeight);
    TowerHeightWeight = FMath::Max(0.0f, TowerHeightWeight);

    CompositeShapeChance = FMath::Clamp(CompositeShapeChance, 0.0f, 1.0f);
    HexagonRotationChance = FMath::Clamp(HexagonRotationChance, 0.0f, 1.0f);
    FlushNeighbourChance = FMath::Clamp(FlushNeighbourChance, 0.0f, 1.0f);
    SidewalkTreeChance = FMath::Clamp(SidewalkTreeChance, 0.0f, 1.0f);
    ParkBlockChance = FMath::Clamp(ParkBlockChance, 0.0f, 0.35f);

    // Park + terraced + detached share one roll; whatever is left over becomes
    // the mixed layout, so the three chances may never exceed 1.
    TerracedBlockChance = FMath::Clamp(TerracedBlockChance, 0.0f, 1.0f);
    DetachedBlockChance = FMath::Clamp(DetachedBlockChance, 0.0f, 1.0f - TerracedBlockChance);

    GrandCityGeometry::OrderRange(SmallFootprintScale, 0.15f, 1.0f);
    GrandCityGeometry::OrderRange(MediumFootprintScale, 0.15f, 1.0f);
    GrandCityGeometry::OrderRange(LargeFootprintScale, 0.15f, 1.0f);
    GrandCityGeometry::OrderRange(ShortHeightRange, 200.0f, 20000.0f);
    GrandCityGeometry::OrderRange(MidHeightRange, 200.0f, 20000.0f);
    GrandCityGeometry::OrderRange(TowerHeightRange, 200.0f, 20000.0f);
    GrandCityGeometry::OrderRange(TerracedGapRange, 0.0f, 1500.0f);
    GrandCityGeometry::OrderRange(DetachedGapRange, 0.0f, 3000.0f);
}

void AGrandCityProceduralCity::RegenerateCity()
{
    if (!CanEditBakedLayout())
    {
        UE_LOG(LogTemp, Warning, TEXT("Regenerate City is editor-only; the baked city is immutable during Play."));
        return;
    }

    SanitizeSettings();

    MarkBakedDataDirty();
    GenerateCity();
    MarkBakedDataDirty();

    UE_LOG(LogTemp, Display,
        TEXT("Baked Grand City '%s' with seed %d: %d buildings (%d parts) = %d small / %d medium / %d large, %d square / %d rectangular / %d hexagon, %d short / %d mid-rise / %d tower, across %d terraced and %d detached blocks. %d road segments, %d sidewalk sections, %d zebra stripes, %d street lights, %d traffic lights, %d trees."),
        *GetName(),
        BakedSeed,
        GenerationStats.LogicalBuildings,
        GenerationStats.BuildingParts,
        GenerationStats.SmallBuildings,
        GenerationStats.MediumBuildings,
        GenerationStats.LargeBuildings,
        GenerationStats.SquareBuildings,
        GenerationStats.RectangularBuildings,
        GenerationStats.HexagonBuildings,
        GenerationStats.ShortBuildings,
        GenerationStats.MidRiseBuildings,
        GenerationStats.TowerBuildings,
        GenerationStats.TerracedBlocks,
        GenerationStats.DetachedBlocks,
        GenerationStats.RoadSegments,
        GenerationStats.SidewalkSections,
        GenerationStats.CrosswalkStripes,
        GenerationStats.StreetLights,
        GenerationStats.TrafficLights,
        GenerationStats.Trees);
}

void AGrandCityProceduralCity::GenerateNewSeed()
{
    if (!CanEditBakedLayout())
    {
        UE_LOG(LogTemp, Warning, TEXT("Generate New Seed is editor-only."));
        return;
    }

    const uint64 Ticks = static_cast<uint64>(FDateTime::UtcNow().GetTicks());
    Seed = static_cast<int32>((Ticks ^ (Ticks >> 32)) & 0x7fffffff);
    RegenerateCity();
}

void AGrandCityProceduralCity::ClearBakedCity()
{
    if (!CanEditBakedLayout())
    {
        UE_LOG(LogTemp, Warning, TEXT("Clear Baked City is editor-only."));
        return;
    }

    MarkBakedDataDirty();
    ClearInstances();
    GenerationStats.Reset();
    bHasBakedLayout = false;
    BakedSeed = 0;
    BakedGeneratorVersion = 0;
    BakedConfigHash = 0;
    MarkBakedDataDirty();
}

void AGrandCityProceduralCity::GenerateCity()
{
    ClearInstances();
    GenerationStats.Reset();
    ApplyVisualMaterials();

    const float Pitch = BlockSize + RoadWidth;
    const float TotalExtent = GridSize * BlockSize + (GridSize + 1) * RoadWidth;

    Ground->SetRelativeLocation(FVector(0.0f, 0.0f, -GrandCityGeometry::GroundThickness * 0.5f));
    Ground->SetRelativeScale3D(FVector(
        (TotalExtent + 1200.0f) / 100.0f,
        (TotalExtent + 1200.0f) / 100.0f,
        GrandCityGeometry::GroundThickness / 100.0f));

    GenerateRoadNetwork(TotalExtent, Pitch);
    GenerateSidewalks();

    for (int32 X = 0; X < GridSize; ++X)
    {
        for (int32 Y = 0; Y < GridSize; ++Y)
        {
            const int32 BlockX = X - GridSize / 2;
            const int32 BlockY = Y - GridSize / 2;
            const FVector2D Center(
                (static_cast<float>(X) - (GridSize - 1) * 0.5f) * Pitch,
                (static_cast<float>(Y) - (GridSize - 1) * 0.5f) * Pitch);

            FRandomStream BuildingRandom(MakeFeatureSeed(BlockX, BlockY, GrandCityGeometry::BuildingSalt));
            GenerateBlock(BlockX, BlockY, Center, BuildingRandom);

            FRandomStream FurnitureRandom(MakeFeatureSeed(BlockX, BlockY, GrandCityGeometry::FurnitureSalt));
            GenerateStreetFurniture(Center, FurnitureRandom);
        }
    }

    GenerateIntersections(Pitch);

    BakedSeed = Seed;
    BakedGeneratorVersion = GeneratorVersion;
    BakedConfigHash = ComputeConfigHash();
    bHasBakedLayout = true;
}

void AGrandCityProceduralCity::GenerateRoadNetwork(float TotalExtent, float Pitch)
{
    const float HalfRoadGrid = GridSize * Pitch * 0.5f;
    const float RoadZ = GrandCityGeometry::RoadThickness * 0.5f;

    // Horizontal corridors are continuous and also cover every intersection.
    for (int32 Line = 0; Line <= GridSize; ++Line)
    {
        const float Y = -HalfRoadGrid + Line * Pitch;
        AddBox(Roads, FVector(0.0f, Y, RoadZ),
            FVector(TotalExtent, RoadWidth, GrandCityGeometry::RoadThickness));
        ++GenerationStats.RoadSegments;

        for (int32 Block = 0; Block < GridSize; ++Block)
        {
            const float X = (static_cast<float>(Block) - (GridSize - 1) * 0.5f) * Pitch;
            AddSegmentMarkings(FVector2D(X, Y), true);
        }
    }

    // Vertical roads are split between horizontal corridors, avoiding
    // coplanar overlapping asphalt at intersections.
    for (int32 Line = 0; Line <= GridSize; ++Line)
    {
        const float X = -HalfRoadGrid + Line * Pitch;
        for (int32 Block = 0; Block < GridSize; ++Block)
        {
            const float Y = (static_cast<float>(Block) - (GridSize - 1) * 0.5f) * Pitch;
            AddBox(Roads, FVector(X, Y, RoadZ),
                FVector(RoadWidth, BlockSize, GrandCityGeometry::RoadThickness));
            ++GenerationStats.RoadSegments;
            AddSegmentMarkings(FVector2D(X, Y), false);
        }
    }
}

void AGrandCityProceduralCity::GenerateSidewalks()
{
    const float Pitch = BlockSize + RoadWidth;
    const float SidewalkZ = GrandCityGeometry::SidewalkHeight * 0.5f;
    const float CurbZ = GrandCityGeometry::CurbHeight * 0.5f;
    const float InnerSideLength = FMath::Max(100.0f, BlockSize - SidewalkWidth * 2.0f);

    for (int32 X = 0; X < GridSize; ++X)
    {
        for (int32 Y = 0; Y < GridSize; ++Y)
        {
            const FVector2D Center(
                (static_cast<float>(X) - (GridSize - 1) * 0.5f) * Pitch,
                (static_cast<float>(Y) - (GridSize - 1) * 0.5f) * Pitch);
            const float Edge = BlockSize * 0.5f;

            AddBox(Sidewalks, FVector(Center.X, Center.Y + Edge - SidewalkWidth * 0.5f, SidewalkZ),
                FVector(BlockSize, SidewalkWidth, GrandCityGeometry::SidewalkHeight));
            AddBox(Sidewalks, FVector(Center.X, Center.Y - Edge + SidewalkWidth * 0.5f, SidewalkZ),
                FVector(BlockSize, SidewalkWidth, GrandCityGeometry::SidewalkHeight));
            AddBox(Sidewalks, FVector(Center.X + Edge - SidewalkWidth * 0.5f, Center.Y, SidewalkZ),
                FVector(SidewalkWidth, InnerSideLength, GrandCityGeometry::SidewalkHeight));
            AddBox(Sidewalks, FVector(Center.X - Edge + SidewalkWidth * 0.5f, Center.Y, SidewalkZ),
                FVector(SidewalkWidth, InnerSideLength, GrandCityGeometry::SidewalkHeight));
            GenerationStats.SidewalkSections += 4;

            AddBox(Curbs, FVector(Center.X, Center.Y + Edge - GrandCityGeometry::CurbDepth * 0.5f, CurbZ),
                FVector(BlockSize, GrandCityGeometry::CurbDepth, GrandCityGeometry::CurbHeight));
            AddBox(Curbs, FVector(Center.X, Center.Y - Edge + GrandCityGeometry::CurbDepth * 0.5f, CurbZ),
                FVector(BlockSize, GrandCityGeometry::CurbDepth, GrandCityGeometry::CurbHeight));
            AddBox(Curbs, FVector(Center.X + Edge - GrandCityGeometry::CurbDepth * 0.5f, Center.Y, CurbZ),
                FVector(GrandCityGeometry::CurbDepth, InnerSideLength, GrandCityGeometry::CurbHeight));
            AddBox(Curbs, FVector(Center.X - Edge + GrandCityGeometry::CurbDepth * 0.5f, Center.Y, CurbZ),
                FVector(GrandCityGeometry::CurbDepth, InnerSideLength, GrandCityGeometry::CurbHeight));
        }
    }
}

void AGrandCityProceduralCity::GenerateIntersections(float Pitch)
{
    const float HalfRoadGrid = GridSize * Pitch * 0.5f;
    const float CrossingOffset = RoadWidth * 0.5f + GrandCityGeometry::CrosswalkWidth * 0.5f + 35.0f;
    const float PoleOffset = RoadWidth * 0.5f + SidewalkWidth * 0.45f;

    // Outer boundaries have no sidewalk on their far side. Crosswalks and
    // signals are therefore emitted only where four city blocks meet.
    for (int32 X = 1; X < GridSize; ++X)
    {
        for (int32 Y = 1; Y < GridSize; ++Y)
        {
            const FVector2D Intersection(
                -HalfRoadGrid + X * Pitch,
                -HalfRoadGrid + Y * Pitch);

            AddCrosswalk(Intersection + FVector2D(0.0f, CrossingOffset), true);
            AddCrosswalk(Intersection + FVector2D(0.0f, -CrossingOffset), true);
            AddCrosswalk(Intersection + FVector2D(CrossingOffset, 0.0f), false);
            AddCrosswalk(Intersection + FVector2D(-CrossingOffset, 0.0f), false);

            for (int32 SX = -1; SX <= 1; SX += 2)
            {
                for (int32 SY = -1; SY <= 1; SY += 2)
                {
                    const FVector2D Position = Intersection + FVector2D(SX * PoleOffset, SY * PoleOffset);
                    const FVector2D Facing = (SX == SY)
                        ? FVector2D(-static_cast<float>(SX), 0.0f)
                        : FVector2D(0.0f, -static_cast<float>(SY));
                    AddTrafficLight(Position, Facing, GrandCityGeometry::SidewalkHeight);
                }
            }
        }
    }
}

void AGrandCityProceduralCity::GenerateBlock(
    int32 BlockX,
    int32 BlockY,
    const FVector2D& Center,
    FRandomStream& Random)
{
    const EGrandCityDistrict District = GetDistrictForBlock(BlockX, BlockY);
    const float BuildableSize = FMath::Max(700.0f,
        BlockSize - 2.0f * (SidewalkWidth + BuildingSetback));
    const float SurfaceSize = FMath::Max(100.0f, BlockSize - SidewalkWidth * 2.0f);
    const float BaseZ = GrandCityGeometry::BlockSurfaceHeight;

    const float EffectiveParkChance = District == EGrandCityDistrict::Downtown
        ? ParkBlockChance * 0.25f
        : ParkBlockChance;

    // One roll decides how densely the block is packed: terraced rows sit wall
    // to wall, detached lots leave courtyards, and every block is separated
    // from its neighbours by the road grid regardless.
    const float LayoutRoll = Random.FRand();
    float Threshold = EffectiveParkChance;

    EBlockLayout Layout = EBlockLayout::Mixed;
    if (LayoutRoll < Threshold)
    {
        Layout = EBlockLayout::Park;
    }
    else
    {
        Threshold += TerracedBlockChance;
        if (LayoutRoll < Threshold)
        {
            Layout = EBlockLayout::TerracedRow;
        }
        else
        {
            Threshold += DetachedBlockChance;
            if (LayoutRoll < Threshold)
            {
                Layout = EBlockLayout::Detached;
            }
        }
    }

    if (Layout == EBlockLayout::Park)
    {
        AddBox(ParkSurfaces,
            FVector(Center.X, Center.Y, GrandCityGeometry::BlockSurfaceHeight * 0.5f),
            FVector(SurfaceSize, SurfaceSize, GrandCityGeometry::BlockSurfaceHeight));

        const int32 ParkTreeCount = Random.RandRange(4, 7);
        for (int32 Index = 0; Index < ParkTreeCount; ++Index)
        {
            const float OffsetX = Random.FRandRange(-BuildableSize * 0.38f, BuildableSize * 0.38f);
            const float OffsetY = Random.FRandRange(-BuildableSize * 0.38f, BuildableSize * 0.38f);
            AddTree(Random, Center + FVector2D(OffsetX, OffsetY), BaseZ);
        }
        return;
    }

    AddBox(BlockSurfaces,
        FVector(Center.X, Center.Y, GrandCityGeometry::BlockSurfaceHeight * 0.5f),
        FVector(SurfaceSize, SurfaceSize, GrandCityGeometry::BlockSurfaceHeight));

    switch (Layout)
    {
        case EBlockLayout::TerracedRow:
            GenerateTerracedRow(Center, BuildableSize, District, Random);
            break;
        case EBlockLayout::Detached:
            GenerateDetachedLots(Center, BuildableSize, District, Random);
            break;
        default:
            GenerateMixedLots(Center, BuildableSize, District, Random);
            break;
    }
}

void AGrandCityProceduralCity::GenerateTerracedRow(
    const FVector2D& Center,
    float BuildableSize,
    EGrandCityDistrict District,
    FRandomStream& Random)
{
    ++GenerationStats.TerracedBlocks;

    const int32 MinimumRowCount = FMath::Max(3, MinBuildingsPerBlock);
    const int32 Count = Random.RandRange(MinimumRowCount, FMath::Max(MinimumRowCount, MaxBuildingsPerBlock));
    const bool bAlongX = Random.RandRange(0, 1) == 0;

    // Gaps are drawn up front so the slot width can absorb them: some
    // neighbours end up flush against each other, others leave an alley.
    TArray<float, TInlineAllocator<8>> Gaps;
    float TotalGap = 0.0f;
    for (int32 Index = 1; Index < Count; ++Index)
    {
        const float Gap = Random.FRand() < FlushNeighbourChance
            ? 0.0f
            : Random.FRandRange(TerracedGapRange.X, TerracedGapRange.Y);
        Gaps.Add(Gap);
        TotalGap += Gap;
    }

    const float Slot = FMath::Max(120.0f, (BuildableSize - TotalGap) / Count);

    // The whole row shares one street frontage line, which is what makes a
    // terrace read as a terrace instead of a scattering of boxes.
    const float FrontSign = Random.RandRange(0, 1) == 0 ? -1.0f : 1.0f;

    float Cursor = -BuildableSize * 0.5f;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if (Index > 0)
        {
            Cursor += Gaps[Index - 1];
        }
        const float Along = Cursor + Slot * 0.5f;
        Cursor += Slot;

        const EBuildingTier Tier = RollBuildingTier(Random);
        const EBuildingHeightClass HeightClass = RollHeightClass(Random);
        const EBuildingShape Shape = RollBuildingShape(Random);
        const int32 StyleIndex = Random.RandRange(0, 3);

        // Depth follows the rolled size class, but never drops below the slot
        // width: the shape pass only ever trims the shorter axis, so keeping
        // the frontage shortest is what preserves the shared wall.
        const float Depth = FMath::Max(BuildableSize * RollFootprintScale(Random, Tier), Slot * 1.02f);
        const float Cross = FrontSign * (BuildableSize - Depth) * 0.5f;

        const FVector2D Local = bAlongX ? FVector2D(Along, Cross) : FVector2D(Cross, Along);
        const FVector2D Footprint = bAlongX ? FVector2D(Slot, Depth) : FVector2D(Depth, Slot);

        AddBuilding(Random, Center + Local, Footprint, GrandCityGeometry::BlockSurfaceHeight,
            Tier, HeightClass, Shape, District, StyleIndex);
    }
}

void AGrandCityProceduralCity::GenerateDetachedLots(
    const FVector2D& Center,
    float BuildableSize,
    EGrandCityDistrict District,
    FRandomStream& Random)
{
    ++GenerationStats.DetachedBlocks;

    const int32 Maximum = FMath::Clamp(MaxBuildingsPerBlock, 1, 3);
    const int32 Count = Random.RandRange(FMath::Min(MinBuildingsPerBlock, Maximum), Maximum);
    const float Gap = Random.FRandRange(DetachedGapRange.X, DetachedGapRange.Y);

    auto Emit = [this, &Random, &Center, District](const FVector2D& Local, float CellX, float CellY)
    {
        const EBuildingTier Tier = RollBuildingTier(Random);
        const EBuildingHeightClass HeightClass = RollHeightClass(Random);
        const EBuildingShape Shape = RollBuildingShape(Random);
        const int32 StyleIndex = Random.RandRange(0, 3);
        const float ScaleX = RollFootprintScale(Random, Tier);
        const float ScaleY = RollFootprintScale(Random, Tier);

        AddBuilding(Random, Center + Local, FVector2D(CellX * ScaleX, CellY * ScaleY),
            GrandCityGeometry::BlockSurfaceHeight, Tier, HeightClass, Shape, District, StyleIndex);
    };

    if (Count <= 1)
    {
        Emit(FVector2D::ZeroVector, BuildableSize, BuildableSize);
        return;
    }

    if (Count == 2)
    {
        const bool bAlongX = Random.RandRange(0, 1) == 0;
        const float Cell = FMath::Max(200.0f, (BuildableSize - Gap) * 0.5f);
        for (int32 Index = 0; Index < 2; ++Index)
        {
            const float Along = (Index == 0 ? -1.0f : 1.0f) * (Cell + Gap) * 0.5f;
            const FVector2D Local = bAlongX ? FVector2D(Along, 0.0f) : FVector2D(0.0f, Along);
            const float CellX = bAlongX ? Cell : BuildableSize;
            const float CellY = bAlongX ? BuildableSize : Cell;
            Emit(Local, CellX, CellY);
        }
        return;
    }

    const float Cell = FMath::Max(200.0f, (BuildableSize - Gap) * 0.5f);
    const float Offset = (Cell + Gap) * 0.5f;
    const FVector2D Positions[3] =
    {
        FVector2D(-Offset, -Offset),
        FVector2D(Offset, -Offset),
        FVector2D(0.0f, Offset)
    };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        Emit(Positions[Index], Cell, Cell);
    }
}

void AGrandCityProceduralCity::GenerateMixedLots(
    const FVector2D& Center,
    float BuildableSize,
    EGrandCityDistrict District,
    FRandomStream& Random)
{
    const int32 Maximum = FMath::Max(3, FMath::Min(4, MaxBuildingsPerBlock));
    const int32 Count = Random.RandRange(3, Maximum);

    // A narrow gap keeps neighbours close without making them share a wall.
    const float Gap = Random.FRandRange(
        FMath::Lerp(static_cast<float>(TerracedGapRange.Y), static_cast<float>(DetachedGapRange.X), 0.25f),
        FMath::Lerp(static_cast<float>(TerracedGapRange.Y), static_cast<float>(DetachedGapRange.X), 1.0f));
    const float Cell = FMath::Max(200.0f, (BuildableSize - Gap) * 0.5f);
    const float Offset = (Cell + Gap) * 0.5f;
    const FVector2D Positions[4] =
    {
        FVector2D(-Offset, -Offset),
        FVector2D(Offset, -Offset),
        FVector2D(-Offset, Offset),
        FVector2D(Offset, Offset)
    };

    for (int32 Index = 0; Index < Count; ++Index)
    {
        const EBuildingTier Tier = RollBuildingTier(Random);
        const EBuildingHeightClass HeightClass = RollHeightClass(Random);
        const EBuildingShape Shape = RollBuildingShape(Random);
        const int32 StyleIndex = Random.RandRange(0, 3);
        const float ScaleX = RollFootprintScale(Random, Tier);
        const float ScaleY = RollFootprintScale(Random, Tier);

        AddBuilding(Random, Center + Positions[Index], FVector2D(Cell * ScaleX, Cell * ScaleY),
            GrandCityGeometry::BlockSurfaceHeight, Tier, HeightClass, Shape, District, StyleIndex);
    }
}

void AGrandCityProceduralCity::GenerateStreetFurniture(const FVector2D& Center, FRandomStream& Random)
{
    const float EdgeInset = BlockSize * 0.5f - SidewalkWidth * 0.48f;
    const float BaseZ = GrandCityGeometry::SidewalkHeight;

    AddStreetLight(Center + FVector2D(0.0f, EdgeInset), FVector2D(0.0f, 1.0f), BaseZ);
    AddStreetLight(Center + FVector2D(0.0f, -EdgeInset), FVector2D(0.0f, -1.0f), BaseZ);
    AddStreetLight(Center + FVector2D(EdgeInset, 0.0f), FVector2D(1.0f, 0.0f), BaseZ);
    AddStreetLight(Center + FVector2D(-EdgeInset, 0.0f), FVector2D(-1.0f, 0.0f), BaseZ);

    const float AlongOffset = BlockSize * 0.28f;
    for (int32 Sign = -1; Sign <= 1; Sign += 2)
    {
        const float JitterA = Random.FRandRange(-55.0f, 55.0f);
        const float JitterB = Random.FRandRange(-55.0f, 55.0f);
        if (Random.FRand() <= SidewalkTreeChance)
        {
            AddTree(Random, Center + FVector2D(Sign * AlongOffset + JitterA, EdgeInset - SidewalkWidth * 0.12f), BaseZ);
        }
        if (Random.FRand() <= SidewalkTreeChance)
        {
            AddTree(Random, Center + FVector2D(Sign * AlongOffset + JitterB, -EdgeInset + SidewalkWidth * 0.12f), BaseZ);
        }
        if (Random.FRand() <= SidewalkTreeChance)
        {
            AddTree(Random, Center + FVector2D(EdgeInset - SidewalkWidth * 0.12f, Sign * AlongOffset + JitterA), BaseZ);
        }
        if (Random.FRand() <= SidewalkTreeChance)
        {
            AddTree(Random, Center + FVector2D(-EdgeInset + SidewalkWidth * 0.12f, Sign * AlongOffset + JitterB), BaseZ);
        }
    }
}

int32 AGrandCityProceduralCity::AddHexagonalPrism(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const FVector2D& Center,
    float CircumRadius,
    float BaseZ,
    float Height,
    float BaseYaw)
{
    if (!Component || CircumRadius <= 1.0f || Height <= 1.0f)
    {
        return 0;
    }

    const float SlabWidth = CircumRadius;
    const float SlabLength = CircumRadius * GrandCityGeometry::HexagonFlatToFlat;

    for (int32 Slab = 0; Slab < 3; ++Slab)
    {
        // The slabs share a base but are stepped by a few millimetres at the
        // top so their coplanar caps do not z-fight with one another.
        const float SlabHeight = FMath::Max(1.0f, Height - Slab * GrandCityGeometry::HexagonSlabStep);
        AddBox(Component,
            FVector(Center.X, Center.Y, BaseZ + SlabHeight * 0.5f),
            FVector(SlabWidth, SlabLength, SlabHeight),
            BaseYaw + Slab * 60.0f);
    }

    return 3;
}

void AGrandCityProceduralCity::AddBuilding(
    FRandomStream& Random,
    const FVector2D& Center,
    const FVector2D& Footprint,
    float BaseZ,
    EBuildingTier Tier,
    EBuildingHeightClass HeightClass,
    EBuildingShape Shape,
    EGrandCityDistrict District,
    int32 StyleIndex)
{
    UHierarchicalInstancedStaticMeshComponent* Body = GetBuildingStyleComponent(StyleIndex);
    if (!Body)
    {
        return;
    }

    RecordBuildingStats(Tier, HeightClass, Shape);
    const float Height = GetBuildingHeight(Random, HeightClass, District);

    FVector2D BodyFootprint = Footprint;
    float HexRadius = 0.0f;
    float HexYaw = 0.0f;

    if (Shape == EBuildingShape::Box)
    {
        // "Kotak": a square plan, taken from the shorter lot axis so the
        // footprint always fits and a terrace frontage stays flush.
        const float Side = FMath::Min(BodyFootprint.X, BodyFootprint.Y);
        BodyFootprint = FVector2D(Side, Side);
    }
    else if (Shape == EBuildingShape::Rectangular)
    {
        // "Persegi panjang": only pull the plan in when it is too close to a
        // square to read as a rectangle. Deep terrace lots already qualify.
        const float TargetRatio = Random.FRandRange(0.45f, 0.72f);
        const float Longer = FMath::Max(BodyFootprint.X, BodyFootprint.Y);
        const float Shorter = FMath::Min(BodyFootprint.X, BodyFootprint.Y);
        if (Shorter > Longer * TargetRatio)
        {
            if (BodyFootprint.X >= BodyFootprint.Y)
            {
                BodyFootprint.Y = Longer * TargetRatio;
            }
            else
            {
                BodyFootprint.X = Longer * TargetRatio;
            }
        }
    }

    auto AddBodyPart = [this, Body](const FVector& PartCenter, const FVector& PartSize)
    {
        AddBox(Body, PartCenter, PartSize);
        ++GenerationStats.BuildingParts;
    };

    switch (Shape)
    {
        case EBuildingShape::Hexagon:
        {
            // Sized off the shorter lot axis so the prism still fits its lot
            // at any yaw.
            HexRadius = 0.5f * FMath::Min(BodyFootprint.X, BodyFootprint.Y);
            HexYaw = Random.FRand() < HexagonRotationChance
                ? Random.FRandRange(0.0f, 60.0f)
                : 0.0f;
            GenerationStats.BuildingParts +=
                AddHexagonalPrism(Body, Center, HexRadius, BaseZ, Height, HexYaw);
            break;
        }

        case EBuildingShape::LShape:
        {
            const float MainWidth = BodyFootprint.X * 0.60f;
            const float WingWidth = BodyFootprint.X * 0.40f;
            const float WingDepth = BodyFootprint.Y * 0.58f;
            AddBodyPart(
                FVector(Center.X - BodyFootprint.X * 0.20f, Center.Y, BaseZ + Height * 0.5f),
                FVector(MainWidth, BodyFootprint.Y, Height));
            AddBodyPart(
                FVector(Center.X + BodyFootprint.X * 0.30f, Center.Y - BodyFootprint.Y * 0.21f, BaseZ + Height * 0.5f),
                FVector(WingWidth, WingDepth, Height));
            break;
        }

        case EBuildingShape::Stepped:
        {
            const float BaseHeight = Height * 0.34f;
            const float MiddleHeight = Height * 0.39f;
            const float TopHeight = Height - BaseHeight - MiddleHeight;
            AddBodyPart(FVector(Center.X, Center.Y, BaseZ + BaseHeight * 0.5f),
                FVector(BodyFootprint.X, BodyFootprint.Y, BaseHeight));
            AddBodyPart(FVector(Center.X, Center.Y, BaseZ + BaseHeight + MiddleHeight * 0.5f),
                FVector(BodyFootprint.X * 0.74f, BodyFootprint.Y * 0.74f, MiddleHeight));
            AddBodyPart(FVector(Center.X, Center.Y, BaseZ + BaseHeight + MiddleHeight + TopHeight * 0.5f),
                FVector(BodyFootprint.X * 0.48f, BodyFootprint.Y * 0.48f, TopHeight));
            break;
        }

        case EBuildingShape::Twin:
        {
            const float BaseHeight = Height * 0.20f;
            const float TowerHeight = Height - BaseHeight;
            const float TowerWidth = BodyFootprint.X * 0.38f;
            AddBodyPart(FVector(Center.X, Center.Y, BaseZ + BaseHeight * 0.5f),
                FVector(BodyFootprint.X, BodyFootprint.Y, BaseHeight));
            AddBodyPart(FVector(Center.X - BodyFootprint.X * 0.27f, Center.Y,
                    BaseZ + BaseHeight + TowerHeight * 0.5f),
                FVector(TowerWidth, BodyFootprint.Y * 0.72f, TowerHeight));
            AddBodyPart(FVector(Center.X + BodyFootprint.X * 0.27f, Center.Y,
                    BaseZ + BaseHeight + TowerHeight * 0.5f),
                FVector(TowerWidth, BodyFootprint.Y * 0.72f, TowerHeight));
            break;
        }

        case EBuildingShape::Box:
        case EBuildingShape::Rectangular:
        default:
            AddBodyPart(FVector(Center.X, Center.Y, BaseZ + Height * 0.5f),
                FVector(BodyFootprint.X, BodyFootprint.Y, Height));
            break;
    }

    // A shallow roof cap makes the silhouettes readable, while occasional
    // utility housings add variety without adding unique meshes.
    if (Shape == EBuildingShape::Hexagon)
    {
        GenerationStats.BuildingParts +=
            AddHexagonalPrism(BuildingRoofs, Center, HexRadius * 0.82f, BaseZ + Height, 18.0f, HexYaw);
    }
    else
    {
        AddBox(BuildingRoofs,
            FVector(Center.X, Center.Y, BaseZ + Height + 9.0f),
            FVector(BodyFootprint.X * 0.78f, BodyFootprint.Y * 0.78f, 18.0f));
        ++GenerationStats.BuildingParts;
    }

    if (Random.FRand() < 0.42f)
    {
        const float UtilityHeight = Random.FRandRange(55.0f, 110.0f);
        AddBox(BuildingRoofs,
            FVector(Center.X, Center.Y, BaseZ + Height + 18.0f + UtilityHeight * 0.5f),
            FVector(BodyFootprint.X * 0.18f, BodyFootprint.Y * 0.18f, UtilityHeight));
        ++GenerationStats.BuildingParts;
    }

    if (bAddFacadePanels)
    {
        AddFacadePanels(Random, Center, BodyFootprint, BaseZ, Height, Shape, HexRadius, HexYaw);
    }
}

void AGrandCityProceduralCity::AddFacadePanels(
    FRandomStream& Random,
    const FVector2D& Center,
    const FVector2D& Footprint,
    float BaseZ,
    float Height,
    EBuildingShape Shape,
    float HexRadius,
    float HexYaw)
{
    float UsableHeight = Height * 0.88f;
    if (Shape == EBuildingShape::Stepped)
    {
        UsableHeight = Height * 0.30f;
    }
    else if (Shape == EBuildingShape::Twin)
    {
        UsableHeight = Height * 0.18f;
    }

    const int32 BandCount = FMath::Clamp(FMath::FloorToInt(UsableHeight / 360.0f), 2, 7);
    const float BandHeight = FMath::Clamp(UsableHeight / (BandCount * 5.0f), 28.0f, 58.0f);
    const bool bUseOppositeSides = Random.FRand() < 0.45f;

    for (int32 Band = 0; Band < BandCount; ++Band)
    {
        const float Z = BaseZ + UsableHeight * (Band + 1.0f) / (BandCount + 1.0f);

        if (Shape == EBuildingShape::Hexagon)
        {
            // A hexagonal ring reuses the same three-slab trick, nudged just
            // outside the body so the band wraps all six faces.
            AddHexagonalPrism(BuildingWindows, Center, HexRadius + 4.5f,
                Z - BandHeight * 0.5f, BandHeight, HexYaw);
            continue;
        }

        AddBox(BuildingWindows,
            FVector(Center.X, Center.Y - Footprint.Y * 0.5f - 4.5f, Z),
            FVector(Footprint.X * 0.72f, 9.0f, BandHeight));
        AddBox(BuildingWindows,
            FVector(Center.X + Footprint.X * 0.5f + 4.5f, Center.Y, Z),
            FVector(9.0f, Footprint.Y * 0.72f, BandHeight));

        if (bUseOppositeSides)
        {
            AddBox(BuildingWindows,
                FVector(Center.X, Center.Y + Footprint.Y * 0.5f + 4.5f, Z),
                FVector(Footprint.X * 0.58f, 9.0f, BandHeight));
        }
    }
}

void AGrandCityProceduralCity::AddTree(FRandomStream& Random, const FVector2D& Position, float BaseZ)
{
    const float TrunkHeight = Random.FRandRange(250.0f, 390.0f);
    const float TrunkDiameter = Random.FRandRange(34.0f, 52.0f);
    const float CrownDiameter = Random.FRandRange(190.0f, 290.0f);
    UHierarchicalInstancedStaticMeshComponent* Crown = Random.FRand() < 0.5f ? TreeCanopiesA : TreeCanopiesB;

    AddCylinder(TreeTrunks,
        FVector(Position.X, Position.Y, BaseZ + TrunkHeight * 0.5f),
        TrunkDiameter,
        TrunkHeight);
    AddSphere(Crown,
        FVector(Position.X, Position.Y, BaseZ + TrunkHeight + CrownDiameter * 0.22f),
        FVector(CrownDiameter, CrownDiameter, CrownDiameter * 0.82f));
    AddSphere(Crown,
        FVector(Position.X + CrownDiameter * 0.16f, Position.Y - CrownDiameter * 0.10f,
            BaseZ + TrunkHeight + CrownDiameter * 0.42f),
        FVector(CrownDiameter * 0.72f, CrownDiameter * 0.72f, CrownDiameter * 0.62f));
    ++GenerationStats.Trees;
}

void AGrandCityProceduralCity::AddStreetLight(
    const FVector2D& Position,
    const FVector2D& TowardRoad,
    float BaseZ)
{
    const FVector2D Direction = TowardRoad.GetSafeNormal();
    const float PoleHeight = 470.0f;
    const float ArmLength = 115.0f;
    const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));

    AddCylinder(StreetLightPoles,
        FVector(Position.X, Position.Y, BaseZ + PoleHeight * 0.5f),
        14.0f,
        PoleHeight);
    AddBox(StreetLightArms,
        FVector(Position.X + Direction.X * ArmLength * 0.5f,
            Position.Y + Direction.Y * ArmLength * 0.5f,
            BaseZ + PoleHeight - 16.0f),
        FVector(ArmLength, 11.0f, 11.0f),
        Yaw);
    AddBox(StreetLightHeads,
        FVector(Position.X + Direction.X * (ArmLength + 10.0f),
            Position.Y + Direction.Y * (ArmLength + 10.0f),
            BaseZ + PoleHeight - 22.0f),
        FVector(46.0f, 30.0f, 15.0f),
        Yaw);
    ++GenerationStats.StreetLights;
}

void AGrandCityProceduralCity::AddTrafficLight(
    const FVector2D& Position,
    const FVector2D& FacingDirection,
    float BaseZ)
{
    const FVector2D Direction = FacingDirection.GetSafeNormal();
    const float PoleHeight = 365.0f;
    const float ArmLength = 105.0f;
    const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
    const FVector2D HeadCenter = Position + Direction * ArmLength;

    AddCylinder(TrafficLightPoles,
        FVector(Position.X, Position.Y, BaseZ + PoleHeight * 0.5f),
        15.0f,
        PoleHeight);
    AddBox(TrafficLightHousings,
        FVector(Position.X + Direction.X * ArmLength * 0.5f,
            Position.Y + Direction.Y * ArmLength * 0.5f,
            BaseZ + PoleHeight - 12.0f),
        FVector(ArmLength, 12.0f, 12.0f),
        Yaw);
    AddBox(TrafficLightHousings,
        FVector(HeadCenter.X, HeadCenter.Y, BaseZ + PoleHeight - 58.0f),
        FVector(34.0f, 52.0f, 122.0f),
        Yaw);

    const FVector2D LensCenter = HeadCenter + Direction * 19.0f;
    AddSphere(TrafficSignalsRed,
        FVector(LensCenter.X, LensCenter.Y, BaseZ + PoleHeight - 26.0f),
        FVector(22.0f, 22.0f, 22.0f));
    AddSphere(TrafficSignalsAmber,
        FVector(LensCenter.X, LensCenter.Y, BaseZ + PoleHeight - 58.0f),
        FVector(22.0f, 22.0f, 22.0f));
    AddSphere(TrafficSignalsGreen,
        FVector(LensCenter.X, LensCenter.Y, BaseZ + PoleHeight - 90.0f),
        FVector(22.0f, 22.0f, 22.0f));
    ++GenerationStats.TrafficLights;
}

void AGrandCityProceduralCity::AddCrosswalk(const FVector2D& Center, bool bCrossesXAxis)
{
    const float TravelSpan = RoadWidth - 100.0f;
    const float Step = TravelSpan / GrandCityGeometry::CrosswalkStripeCount;
    const float StripeLength = Step * 0.58f;
    const float Z = GrandCityGeometry::RoadThickness + GrandCityGeometry::MarkingHeight * 0.5f;

    for (int32 Stripe = 0; Stripe < GrandCityGeometry::CrosswalkStripeCount; ++Stripe)
    {
        const float Offset = -TravelSpan * 0.5f + Step * (Stripe + 0.5f);
        const FVector Location = bCrossesXAxis
            ? FVector(Center.X + Offset, Center.Y, Z)
            : FVector(Center.X, Center.Y + Offset, Z);
        const FVector Size = bCrossesXAxis
            ? FVector(StripeLength, GrandCityGeometry::CrosswalkWidth, GrandCityGeometry::MarkingHeight)
            : FVector(GrandCityGeometry::CrosswalkWidth, StripeLength, GrandCityGeometry::MarkingHeight);
        AddBox(Crosswalks, Location, Size);
        ++GenerationStats.CrosswalkStripes;
    }
}

void AGrandCityProceduralCity::AddSegmentMarkings(const FVector2D& Center, bool bRunsAlongXAxis)
{
    const float Z = GrandCityGeometry::RoadThickness + GrandCityGeometry::MarkingHeight * 0.5f;
    const float EdgeOffset = RoadWidth * 0.38f;
    const float EdgeLength = BlockSize - 90.0f;
    const FVector EdgeSize = bRunsAlongXAxis
        ? FVector(EdgeLength, 10.0f, GrandCityGeometry::MarkingHeight)
        : FVector(10.0f, EdgeLength, GrandCityGeometry::MarkingHeight);

    if (bRunsAlongXAxis)
    {
        AddBox(RoadMarkings, FVector(Center.X, Center.Y + EdgeOffset, Z), EdgeSize);
        AddBox(RoadMarkings, FVector(Center.X, Center.Y - EdgeOffset, Z), EdgeSize);
    }
    else
    {
        AddBox(RoadMarkings, FVector(Center.X + EdgeOffset, Center.Y, Z), EdgeSize);
        AddBox(RoadMarkings, FVector(Center.X - EdgeOffset, Center.Y, Z), EdgeSize);
    }

    const float EndMargin = FMath::Min(430.0f, BlockSize * 0.22f);
    const float UsableLength = FMath::Max(300.0f, BlockSize - EndMargin * 2.0f);
    const int32 DashCount = FMath::Max(2, FMath::FloorToInt(UsableLength / 360.0f));
    const float DashStep = UsableLength / DashCount;

    for (int32 Dash = 0; Dash < DashCount; ++Dash)
    {
        const float Along = -UsableLength * 0.5f + DashStep * (Dash + 0.5f);
        const FVector Location = bRunsAlongXAxis
            ? FVector(Center.X + Along, Center.Y, Z)
            : FVector(Center.X, Center.Y + Along, Z);
        const FVector Size = bRunsAlongXAxis
            ? FVector(DashStep * 0.46f, 14.0f, GrandCityGeometry::MarkingHeight)
            : FVector(14.0f, DashStep * 0.46f, GrandCityGeometry::MarkingHeight);
        AddBox(RoadMarkings, Location, Size);
    }
}

void AGrandCityProceduralCity::AddBox(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const FVector& Center,
    const FVector& Size,
    float YawDegrees)
{
    if (!Component || !Component->GetStaticMesh())
    {
        return;
    }

    Component->AddInstance(FTransform(
        FRotator(0.0f, YawDegrees, 0.0f),
        Center,
        FVector(
            FMath::Max(0.01f, Size.X / 100.0f),
            FMath::Max(0.01f, Size.Y / 100.0f),
            FMath::Max(0.01f, Size.Z / 100.0f))));
}

void AGrandCityProceduralCity::AddCylinder(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const FVector& Center,
    float Diameter,
    float Height,
    float YawDegrees)
{
    if (!Component || !Component->GetStaticMesh())
    {
        return;
    }

    Component->AddInstance(FTransform(
        FRotator(0.0f, YawDegrees, 0.0f),
        Center,
        FVector(Diameter / 100.0f, Diameter / 100.0f, Height / 100.0f)));
}

void AGrandCityProceduralCity::AddSphere(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const FVector& Center,
    const FVector& Diameter)
{
    if (!Component || !Component->GetStaticMesh())
    {
        return;
    }

    Component->AddInstance(FTransform(
        FRotator::ZeroRotator,
        Center,
        FVector(Diameter.X / 100.0f, Diameter.Y / 100.0f, Diameter.Z / 100.0f)));
}

AGrandCityProceduralCity::EBuildingTier AGrandCityProceduralCity::RollBuildingTier(FRandomStream& Random) const
{
    const float Weights[3] = { SmallBuildingWeight, MediumBuildingWeight, LargeBuildingWeight };
    switch (GrandCityGeometry::PickWeighted(Random, Weights, 3))
    {
        case 0: return EBuildingTier::Small;
        case 1: return EBuildingTier::Medium;
        default: return EBuildingTier::Large;
    }
}

AGrandCityProceduralCity::EBuildingHeightClass AGrandCityProceduralCity::RollHeightClass(FRandomStream& Random) const
{
    const float Weights[3] = { ShortHeightWeight, MidHeightWeight, TowerHeightWeight };
    switch (GrandCityGeometry::PickWeighted(Random, Weights, 3))
    {
        case 0: return EBuildingHeightClass::Short;
        case 1: return EBuildingHeightClass::MidRise;
        default: return EBuildingHeightClass::Tower;
    }
}

AGrandCityProceduralCity::EBuildingShape AGrandCityProceduralCity::RollBuildingShape(FRandomStream& Random) const
{
    // The composite silhouettes stay off by default so the prototype reads as
    // the three requested plans: square, rectangle and hexagon.
    const float CompositeRoll = Random.FRand();
    if (CompositeRoll < CompositeShapeChance)
    {
        switch (Random.RandRange(0, 2))
        {
            case 0: return EBuildingShape::LShape;
            case 1: return EBuildingShape::Stepped;
            default: return EBuildingShape::Twin;
        }
    }

    const float Weights[3] = { BoxShapeWeight, RectangleShapeWeight, HexagonShapeWeight };
    switch (GrandCityGeometry::PickWeighted(Random, Weights, 3))
    {
        case 0: return EBuildingShape::Box;
        case 1: return EBuildingShape::Rectangular;
        default: return EBuildingShape::Hexagon;
    }
}

float AGrandCityProceduralCity::RollFootprintScale(FRandomStream& Random, EBuildingTier Tier) const
{
    switch (Tier)
    {
        case EBuildingTier::Small:
            return Random.FRandRange(SmallFootprintScale.X, SmallFootprintScale.Y);
        case EBuildingTier::Medium:
            return Random.FRandRange(MediumFootprintScale.X, MediumFootprintScale.Y);
        default:
            return Random.FRandRange(LargeFootprintScale.X, LargeFootprintScale.Y);
    }
}

float AGrandCityProceduralCity::GetBuildingHeight(
    FRandomStream& Random,
    EBuildingHeightClass HeightClass,
    EGrandCityDistrict District) const
{
    float Height = 1000.0f;
    switch (HeightClass)
    {
        case EBuildingHeightClass::Short:
            Height = Random.FRandRange(ShortHeightRange.X, ShortHeightRange.Y);
            break;
        case EBuildingHeightClass::MidRise:
            Height = Random.FRandRange(MidHeightRange.X, MidHeightRange.Y);
            break;
        case EBuildingHeightClass::Tower:
            Height = Random.FRandRange(TowerHeightRange.X, TowerHeightRange.Y);
            break;
    }

    if (bApplyDistrictHeightBias)
    {
        switch (District)
        {
            case EGrandCityDistrict::Downtown:
                Height *= 1.20f;
                break;
            case EGrandCityDistrict::Commercial:
                Height *= 1.08f;
                break;
            case EGrandCityDistrict::Industrial:
                Height *= 0.72f;
                break;
            case EGrandCityDistrict::Residential:
                Height *= 0.85f;
                break;
            case EGrandCityDistrict::Services:
                Height *= 0.95f;
                break;
        }
    }

    return FMath::Clamp(Height, 250.0f, 22000.0f);
}

void AGrandCityProceduralCity::RecordBuildingStats(
    EBuildingTier Tier,
    EBuildingHeightClass HeightClass,
    EBuildingShape Shape)
{
    ++GenerationStats.LogicalBuildings;

    switch (Tier)
    {
        case EBuildingTier::Small: ++GenerationStats.SmallBuildings; break;
        case EBuildingTier::Medium: ++GenerationStats.MediumBuildings; break;
        default: ++GenerationStats.LargeBuildings; break;
    }

    switch (HeightClass)
    {
        case EBuildingHeightClass::Short: ++GenerationStats.ShortBuildings; break;
        case EBuildingHeightClass::MidRise: ++GenerationStats.MidRiseBuildings; break;
        default: ++GenerationStats.TowerBuildings; break;
    }

    switch (Shape)
    {
        case EBuildingShape::Box: ++GenerationStats.SquareBuildings; break;
        case EBuildingShape::Rectangular: ++GenerationStats.RectangularBuildings; break;
        case EBuildingShape::Hexagon: ++GenerationStats.HexagonBuildings; break;
        default: break;
    }
}

EGrandCityDistrict AGrandCityProceduralCity::GetDistrictForBlock(int32 X, int32 Y) const
{
    const int32 Distance = FMath::Max(FMath::Abs(X), FMath::Abs(Y));
    if (Distance <= 1)
    {
        return EGrandCityDistrict::Downtown;
    }
    if (X <= -2)
    {
        return EGrandCityDistrict::Industrial;
    }
    if (Y >= 2)
    {
        return EGrandCityDistrict::Commercial;
    }
    if (X >= 2)
    {
        return EGrandCityDistrict::Services;
    }
    return EGrandCityDistrict::Residential;
}

int32 AGrandCityProceduralCity::MakeFeatureSeed(int32 BlockX, int32 BlockY, uint32 Salt) const
{
    uint32 Hash = HashCombine(GetTypeHash(Seed), GetTypeHash(BlockX));
    Hash = HashCombine(Hash, GetTypeHash(BlockY));
    Hash = HashCombine(Hash, Salt);
    return static_cast<int32>(Hash & 0x7fffffffu);
}

int32 AGrandCityProceduralCity::ComputeConfigHash() const
{
    uint32 Hash = GetTypeHash(GeneratorVersion);
    Hash = HashCombine(Hash, GetTypeHash(Seed));
    Hash = HashCombine(Hash, GetTypeHash(GridSize));
    Hash = HashCombine(Hash, GetTypeHash(BlockSize));
    Hash = HashCombine(Hash, GetTypeHash(RoadWidth));
    Hash = HashCombine(Hash, GetTypeHash(SidewalkWidth));
    Hash = HashCombine(Hash, GetTypeHash(BuildingSetback));
    Hash = HashCombine(Hash, GetTypeHash(MinBuildingsPerBlock));
    Hash = HashCombine(Hash, GetTypeHash(MaxBuildingsPerBlock));
    Hash = HashCombine(Hash, GetTypeHash(bAddFacadePanels));

    Hash = HashCombine(Hash, GetTypeHash(SmallBuildingWeight));
    Hash = HashCombine(Hash, GetTypeHash(MediumBuildingWeight));
    Hash = HashCombine(Hash, GetTypeHash(LargeBuildingWeight));
    Hash = HashCombine(Hash, GetTypeHash(SmallFootprintScale));
    Hash = HashCombine(Hash, GetTypeHash(MediumFootprintScale));
    Hash = HashCombine(Hash, GetTypeHash(LargeFootprintScale));

    Hash = HashCombine(Hash, GetTypeHash(BoxShapeWeight));
    Hash = HashCombine(Hash, GetTypeHash(RectangleShapeWeight));
    Hash = HashCombine(Hash, GetTypeHash(HexagonShapeWeight));
    Hash = HashCombine(Hash, GetTypeHash(CompositeShapeChance));
    Hash = HashCombine(Hash, GetTypeHash(HexagonRotationChance));

    Hash = HashCombine(Hash, GetTypeHash(ShortHeightWeight));
    Hash = HashCombine(Hash, GetTypeHash(MidHeightWeight));
    Hash = HashCombine(Hash, GetTypeHash(TowerHeightWeight));
    Hash = HashCombine(Hash, GetTypeHash(ShortHeightRange));
    Hash = HashCombine(Hash, GetTypeHash(MidHeightRange));
    Hash = HashCombine(Hash, GetTypeHash(TowerHeightRange));
    Hash = HashCombine(Hash, GetTypeHash(bApplyDistrictHeightBias));

    Hash = HashCombine(Hash, GetTypeHash(TerracedBlockChance));
    Hash = HashCombine(Hash, GetTypeHash(DetachedBlockChance));
    Hash = HashCombine(Hash, GetTypeHash(ParkBlockChance));
    Hash = HashCombine(Hash, GetTypeHash(TerracedGapRange));
    Hash = HashCombine(Hash, GetTypeHash(DetachedGapRange));
    Hash = HashCombine(Hash, GetTypeHash(FlushNeighbourChance));
    Hash = HashCombine(Hash, GetTypeHash(SidewalkTreeChance));
    return static_cast<int32>(Hash & 0x7fffffffu);
}

TArray<UHierarchicalInstancedStaticMeshComponent*> AGrandCityProceduralCity::GetAllInstanceComponents() const
{
    return
    {
        Roads,
        BlockSurfaces,
        ParkSurfaces,
        Sidewalks,
        Curbs,
        RoadMarkings,
        Crosswalks,
        BuildingStyleA,
        BuildingStyleB,
        BuildingStyleC,
        BuildingStyleD,
        BuildingRoofs,
        BuildingWindows,
        StreetLightPoles,
        StreetLightArms,
        StreetLightHeads,
        TrafficLightPoles,
        TrafficLightHousings,
        TrafficSignalsRed,
        TrafficSignalsAmber,
        TrafficSignalsGreen,
        TreeTrunks,
        TreeCanopiesA,
        TreeCanopiesB
    };
}

UHierarchicalInstancedStaticMeshComponent* AGrandCityProceduralCity::GetBuildingStyleComponent(int32 StyleIndex) const
{
    switch (StyleIndex & 3)
    {
        case 0: return BuildingStyleA;
        case 1: return BuildingStyleB;
        case 2: return BuildingStyleC;
        default: return BuildingStyleD;
    }
}

void AGrandCityProceduralCity::ClearInstances()
{
    for (UHierarchicalInstancedStaticMeshComponent* Component : GetAllInstanceComponents())
    {
        if (Component)
        {
            Component->ClearInstances();
        }
    }
}

void AGrandCityProceduralCity::ApplyVisualMaterials()
{
    ApplyColorMaterial(Ground, FlatColorMaterial, FLinearColor(0.075f, 0.11f, 0.065f), 0.95f);
    ApplyColorMaterial(Roads, FlatColorMaterial, FLinearColor(0.025f, 0.030f, 0.038f), 0.92f);
    ApplyColorMaterial(BlockSurfaces, FlatColorMaterial, FLinearColor(0.19f, 0.20f, 0.18f), 0.92f);
    ApplyColorMaterial(ParkSurfaces, FlatColorMaterial, FLinearColor(0.075f, 0.20f, 0.065f), 0.96f);
    ApplyColorMaterial(Sidewalks, FlatColorMaterial, FLinearColor(0.46f, 0.48f, 0.49f), 0.88f);
    ApplyColorMaterial(Curbs, FlatColorMaterial, FLinearColor(0.68f, 0.69f, 0.67f), 0.90f);
    ApplyColorMaterial(RoadMarkings, FlatColorMaterial, FLinearColor(0.93f, 0.94f, 0.90f), 0.78f);
    ApplyColorMaterial(Crosswalks, FlatColorMaterial, FLinearColor(0.98f, 0.98f, 0.95f), 0.74f);

    ApplyColorMaterial(BuildingStyleA, FlatColorMaterial, FLinearColor(0.48f, 0.20f, 0.12f), 0.72f);
    ApplyColorMaterial(BuildingStyleB, FlatColorMaterial, FLinearColor(0.18f, 0.29f, 0.38f), 0.62f, 0.04f);
    ApplyColorMaterial(BuildingStyleC, FlatColorMaterial, FLinearColor(0.58f, 0.51f, 0.38f), 0.80f);
    ApplyColorMaterial(BuildingStyleD, FlatColorMaterial, FLinearColor(0.23f, 0.24f, 0.27f), 0.66f, 0.08f);
    ApplyColorMaterial(BuildingRoofs, FlatColorMaterial, FLinearColor(0.085f, 0.09f, 0.10f), 0.76f);
    ApplyColorMaterial(BuildingWindows, FlatColorMaterial, FLinearColor(0.025f, 0.16f, 0.23f), 0.18f, 0.45f);

    ApplyColorMaterial(StreetLightPoles, FlatColorMaterial, FLinearColor(0.055f, 0.06f, 0.07f), 0.46f, 0.55f);
    ApplyColorMaterial(StreetLightArms, FlatColorMaterial, FLinearColor(0.055f, 0.06f, 0.07f), 0.46f, 0.55f);
    ApplyColorMaterial(StreetLightHeads, GlowMaterial ? GlowMaterial : FlatColorMaterial,
        FLinearColor(1.0f, 0.72f, 0.27f), 0.30f, 0.0f, 8.0f);

    ApplyColorMaterial(TrafficLightPoles, FlatColorMaterial, FLinearColor(0.08f, 0.085f, 0.09f), 0.58f, 0.35f);
    ApplyColorMaterial(TrafficLightHousings, FlatColorMaterial, FLinearColor(0.035f, 0.038f, 0.04f), 0.70f);
    ApplyColorMaterial(TrafficSignalsRed, GlowMaterial ? GlowMaterial : FlatColorMaterial,
        FLinearColor(1.0f, 0.015f, 0.01f), 0.25f, 0.0f, 7.0f);
    ApplyColorMaterial(TrafficSignalsAmber, GlowMaterial ? GlowMaterial : FlatColorMaterial,
        FLinearColor(1.0f, 0.34f, 0.01f), 0.25f, 0.0f, 5.0f);
    ApplyColorMaterial(TrafficSignalsGreen, GlowMaterial ? GlowMaterial : FlatColorMaterial,
        FLinearColor(0.015f, 0.80f, 0.08f), 0.25f, 0.0f, 5.0f);

    ApplyColorMaterial(TreeTrunks, FlatColorMaterial, FLinearColor(0.18f, 0.075f, 0.028f), 0.96f);
    ApplyColorMaterial(TreeCanopiesA, FlatColorMaterial, FLinearColor(0.035f, 0.25f, 0.055f), 0.98f);
    ApplyColorMaterial(TreeCanopiesB, FlatColorMaterial, FLinearColor(0.09f, 0.36f, 0.075f), 0.98f);
}

void AGrandCityProceduralCity::ApplyColorMaterial(
    UPrimitiveComponent* Component,
    UMaterialInterface* Parent,
    const FLinearColor& Color,
    float Roughness,
    float Metallic,
    float Glow)
{
    if (!Component || !Parent)
    {
        return;
    }

    UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, Component);
    if (!Material)
    {
        return;
    }

    Material->SetFlags(RF_Transient);
    Material->SetVectorParameterValue(TEXT("Color"), Color);
    Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
    // M_FlatCol names its color parameter with a space.
    Material->SetVectorParameterValue(TEXT("Base Color"), Color);
    Material->SetVectorParameterValue(TEXT("GlowColor"), Color);
    Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
    Material->SetScalarParameterValue(TEXT("Metallic"), Metallic);
    Material->SetScalarParameterValue(TEXT("Glow"), Glow);
    Material->SetScalarParameterValue(TEXT("EmissiveStrength"), Glow);
    Component->SetMaterial(0, Material);
}

bool AGrandCityProceduralCity::CanEditBakedLayout() const
{
    const UWorld* World = GetWorld();
    return !World || !World->IsGameWorld();
}

void AGrandCityProceduralCity::MarkBakedDataDirty()
{
    Modify();
    if (Ground)
    {
        Ground->Modify();
    }
    for (UHierarchicalInstancedStaticMeshComponent* Component : GetAllInstanceComponents())
    {
        if (Component)
        {
            Component->Modify();
        }
    }
    MarkPackageDirty();
}
