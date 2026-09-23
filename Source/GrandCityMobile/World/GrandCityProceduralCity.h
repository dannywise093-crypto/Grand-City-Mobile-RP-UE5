#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrandCityProceduralCity.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMesh;
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

USTRUCT(BlueprintType)
struct FGrandCityGenerationStats
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 LogicalBuildings = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 BuildingParts = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 SmallBuildings = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 MediumBuildings = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 LargeBuildings = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 SquareBuildings = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 RectangularBuildings = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 HexagonBuildings = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 ShortBuildings = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 MidRiseBuildings = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 TowerBuildings = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 TerracedBlocks = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 DetachedBlocks = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 RoadSegments = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 SidewalkSections = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 CrosswalkStripes = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 StreetLights = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 TrafficLights = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City")
    int32 Trees = 0;

    void Reset()
    {
        *this = FGrandCityGenerationStats();
    }
};

/**
 * Deterministic, editor-baked city generator.
 *
 * Regenerate City is intentionally the only path that changes the layout.
 * The generated HISM data is serialized into the level and BeginPlay only
 * validates it, so pressing Play never clears or refreshes the city.
 */
UCLASS()
class GRANDCITYMOBILE_API AGrandCityProceduralCity : public AActor
{
    GENERATED_BODY()

public:
    AGrandCityProceduralCity();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void PostActorCreated() override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    /** Rebuilds and bakes the city using the current seed and settings. */
    UFUNCTION(CallInEditor, Category="Grand City|Generation")
    void RegenerateCity();

    /** Chooses a new seed, then explicitly rebuilds the baked city. */
    UFUNCTION(CallInEditor, Category="Grand City|Generation")
    void GenerateNewSeed();

    /** Removes all baked instances. Nothing is regenerated at runtime. */
    UFUNCTION(CallInEditor, Category="Grand City|Generation")
    void ClearBakedCity();

    UFUNCTION(BlueprintPure, Category="Grand City|Generation")
    bool HasBakedLayout() const { return bHasBakedLayout; }

    UFUNCTION(BlueprintPure, Category="Grand City|Generation")
    int32 GetBakedSeed() const { return BakedSeed; }

    UFUNCTION(BlueprintPure, Category="Grand City|Generation")
    EGrandCityDistrict GetDistrictForBlock(int32 X, int32 Y) const;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<USceneComponent> CityRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UStaticMeshComponent> Ground;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Roads;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> BlockSurfaces;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ParkSurfaces;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Sidewalks;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Curbs;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RoadMarkings;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Crosswalks;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> BuildingStyleA;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> BuildingStyleB;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> BuildingStyleC;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> BuildingStyleD;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> BuildingRoofs;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> BuildingWindows;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> StreetLightPoles;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> StreetLightArms;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> StreetLightHeads;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TrafficLightPoles;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TrafficLightHousings;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TrafficSignalsRed;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TrafficSignalsAmber;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TrafficSignalsGreen;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TreeTrunks;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TreeCanopiesA;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Components")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TreeCanopiesB;

    /** Number of blocks along each axis. Roads are emitted on every boundary. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Layout", meta=(ClampMin="2", ClampMax="12"))
    int32 GridSize = 6;

    /** Clear width/length of a block between road edges, in centimetres. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Layout", meta=(ClampMin="1800.0", ClampMax="8000.0", Units="cm"))
    float BlockSize = 3200.0f;

    /** Connected two-lane road corridor width, in centimetres. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Layout", meta=(ClampMin="700.0", ClampMax="2400.0", Units="cm"))
    float RoadWidth = 1200.0f;

    /** Sidewalk inset placed between every building lot and road. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Layout", meta=(ClampMin="160.0", ClampMax="600.0", Units="cm"))
    float SidewalkWidth = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Layout", meta=(ClampMin="40.0", ClampMax="500.0", Units="cm"))
    float BuildingSetback = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings", meta=(ClampMin="1", ClampMax="4"))
    int32 MinBuildingsPerBlock = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings", meta=(ClampMin="2", ClampMax="7"))
    int32 MaxBuildingsPerBlock = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings")
    bool bAddFacadePanels = true;

    // ---------------------------------------------------------------------
    // Footprint size lottery: small / medium / large. Weights are relative,
    // so 1 / 1.6 / 0.9 simply means medium comes up most often.
    // ---------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Size", meta=(ClampMin="0.0"))
    float SmallBuildingWeight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Size", meta=(ClampMin="0.0"))
    float MediumBuildingWeight = 1.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Size", meta=(ClampMin="0.0"))
    float LargeBuildingWeight = 0.9f;

    /** Fraction of the available lot a small building may occupy (X=min, Y=max). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Size")
    FVector2D SmallFootprintScale = FVector2D(0.40f, 0.58f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Size")
    FVector2D MediumFootprintScale = FVector2D(0.58f, 0.78f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Size")
    FVector2D LargeFootprintScale = FVector2D(0.78f, 0.97f);

    // ---------------------------------------------------------------------
    // Shape lottery: square box / rectangle / hexagon.
    // ---------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Shape", meta=(ClampMin="0.0"))
    float BoxShapeWeight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Shape", meta=(ClampMin="0.0"))
    float RectangleShapeWeight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Shape", meta=(ClampMin="0.0"))
    float HexagonShapeWeight = 0.5f;

    /**
     * Chance to roll one of the composite silhouettes (L-shape / stepped /
     * twin tower) instead of a basic shape. Zero keeps the prototype limited
     * to squares, rectangles and hexagons.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Shape", meta=(ClampMin="0.0", ClampMax="1.0"))
    float CompositeShapeChance = 0.0f;

    /** Chance a hexagon gets a random yaw instead of staying axis aligned. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Shape", meta=(ClampMin="0.0", ClampMax="1.0"))
    float HexagonRotationChance = 0.65f;

    // ---------------------------------------------------------------------
    // Height lottery: short / mid-rise / skyscraper. Rolled independently of
    // the footprint, so a small lot can still carry a slim tower.
    // ---------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Height", meta=(ClampMin="0.0"))
    float ShortHeightWeight = 1.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Height", meta=(ClampMin="0.0"))
    float MidHeightWeight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Height", meta=(ClampMin="0.0"))
    float TowerHeightWeight = 0.35f;

    /** Height range in centimetres for short buildings (X=min, Y=max). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Height", meta=(Units="cm"))
    FVector2D ShortHeightRange = FVector2D(450.0f, 1100.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Height", meta=(Units="cm"))
    FVector2D MidHeightRange = FVector2D(1300.0f, 2600.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Height", meta=(Units="cm"))
    FVector2D TowerHeightRange = FVector2D(3200.0f, 7200.0f);

    /** Districts nudge the rolled height so downtown still reads as a core. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Height")
    bool bApplyDistrictHeightBias = true;

    // ---------------------------------------------------------------------
    // Spacing lottery: terraced row (touching) vs detached (wide gaps).
    // Blocks themselves are always separated by the road grid.
    // ---------------------------------------------------------------------

    /** Chance a block is a tightly packed terraced row. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Spacing", meta=(ClampMin="0.0", ClampMax="1.0"))
    float TerracedBlockChance = 0.45f;

    /** Chance a block uses a few widely separated free-standing buildings. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Spacing", meta=(ClampMin="0.0", ClampMax="1.0"))
    float DetachedBlockChance = 0.30f;

    /** Chance a block is intentionally open as a small park/plaza. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Spacing", meta=(ClampMin="0.0", ClampMax="0.35"))
    float ParkBlockChance = 0.08f;

    /** Gap between neighbours in a terraced row (X=min, Y=max). Zero is flush. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Spacing", meta=(Units="cm"))
    FVector2D TerracedGapRange = FVector2D(0.0f, 150.0f);

    /** Gap between free-standing buildings inside one block (X=min, Y=max). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Spacing", meta=(Units="cm"))
    FVector2D DetachedGapRange = FVector2D(380.0f, 720.0f);

    /** Chance a terraced neighbour sits flush against the previous one. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Buildings|Spacing", meta=(ClampMin="0.0", ClampMax="1.0"))
    float FlushNeighbourChance = 0.45f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Street Furniture", meta=(ClampMin="0.0", ClampMax="1.0"))
    float SidewalkTreeChance = 0.82f;

    /** Stable seed. It only takes effect after Regenerate City is pressed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Generation")
    int32 Seed = 20260921;

    /**
     * Editor-only trigger. Ticking it bakes the city once and immediately
     * clears itself, so external tooling can request a bake by setting a
     * single property.
     */
    UPROPERTY(EditAnywhere, Category="City|Generation")
    bool bRequestRegenerate = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="City|Generation")
    bool bHasBakedLayout = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="City|Generation")
    int32 BakedSeed = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="City|Generation")
    int32 BakedGeneratorVersion = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="City|Generation")
    int32 BakedConfigHash = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="City|Generation")
    FGrandCityGenerationStats GenerationStats;

private:
    /** Footprint class drawn per building. */
    enum class EBuildingTier : uint8
    {
        Small,
        Medium,
        Large
    };

    /** Height class drawn per building, independent of the footprint class. */
    enum class EBuildingHeightClass : uint8
    {
        Short,
        MidRise,
        Tower
    };

    enum class EBuildingShape : uint8
    {
        Box,
        Rectangular,
        Hexagon,
        LShape,
        Stepped,
        Twin
    };

    enum class EBlockLayout : uint8
    {
        TerracedRow,
        Detached,
        Mixed,
        Park
    };

    static constexpr int32 GeneratorVersion = 4;

    UPROPERTY()
    TObjectPtr<UStaticMesh> CubeMesh;

    UPROPERTY()
    TObjectPtr<UStaticMesh> CylinderMesh;

    UPROPERTY()
    TObjectPtr<UStaticMesh> SphereMesh;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> FlatColorMaterial;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> GlowMaterial;

    UHierarchicalInstancedStaticMeshComponent* CreateInstancedComponent(
        const FName& Name,
        UStaticMesh* Mesh,
        ECollisionEnabled::Type Collision,
        bool bAffectsNavigation,
        bool bCastsShadow = true);

    TArray<UHierarchicalInstancedStaticMeshComponent*> GetAllInstanceComponents() const;
    UHierarchicalInstancedStaticMeshComponent* GetBuildingStyleComponent(int32 StyleIndex) const;

    void ApplyVisualMaterials();
    void ApplyColorMaterial(UPrimitiveComponent* Component, UMaterialInterface* Parent,
        const FLinearColor& Color, float Roughness, float Metallic = 0.0f, float Glow = 0.0f);
    void ClearInstances();
    void SanitizeSettings();
    void GenerateCity();
    void GenerateRoadNetwork(float TotalExtent, float Pitch);
    void GenerateSidewalks();
    void GenerateIntersections(float Pitch);
    void GenerateBlock(int32 BlockX, int32 BlockY, const FVector2D& Center, FRandomStream& Random);
    void GenerateTerracedRow(const FVector2D& Center, float BuildableSize,
        EGrandCityDistrict District, FRandomStream& Random);
    void GenerateDetachedLots(const FVector2D& Center, float BuildableSize,
        EGrandCityDistrict District, FRandomStream& Random);
    void GenerateMixedLots(const FVector2D& Center, float BuildableSize,
        EGrandCityDistrict District, FRandomStream& Random);
    void GenerateStreetFurniture(const FVector2D& Center, FRandomStream& Random);

    void AddBox(UHierarchicalInstancedStaticMeshComponent* Component, const FVector& Center,
        const FVector& Size, float YawDegrees = 0.0f);
    void AddCylinder(UHierarchicalInstancedStaticMeshComponent* Component, const FVector& Center,
        float Diameter, float Height, float YawDegrees = 0.0f);
    void AddSphere(UHierarchicalInstancedStaticMeshComponent* Component, const FVector& Center,
        const FVector& Diameter);

    /**
     * Emits a regular hexagonal prism built from three rotated boxes.
     *
     * The union of three slabs sized CircumRadius x CircumRadius*sqrt(3) and
     * placed 60 degrees apart is exactly a regular hexagon, so a hexagonal
     * footprint needs no custom mesh, only the engine cube.
     */
    int32 AddHexagonalPrism(UHierarchicalInstancedStaticMeshComponent* Component,
        const FVector2D& Center, float CircumRadius, float BaseZ, float Height, float BaseYaw);

    void AddBuilding(FRandomStream& Random, const FVector2D& Center, const FVector2D& Footprint,
        float BaseZ, EBuildingTier Tier, EBuildingHeightClass HeightClass, EBuildingShape Shape,
        EGrandCityDistrict District, int32 StyleIndex);
    void AddFacadePanels(FRandomStream& Random, const FVector2D& Center, const FVector2D& Footprint,
        float BaseZ, float Height, EBuildingShape Shape, float HexRadius, float HexYaw);
    void AddTree(FRandomStream& Random, const FVector2D& Position, float BaseZ);
    void AddStreetLight(const FVector2D& Position, const FVector2D& TowardRoad, float BaseZ);
    void AddTrafficLight(const FVector2D& Position, const FVector2D& FacingDirection, float BaseZ);
    void AddCrosswalk(const FVector2D& Center, bool bCrossesXAxis);
    void AddSegmentMarkings(const FVector2D& Center, bool bRunsAlongXAxis);

    EBuildingTier RollBuildingTier(FRandomStream& Random) const;
    EBuildingHeightClass RollHeightClass(FRandomStream& Random) const;
    EBuildingShape RollBuildingShape(FRandomStream& Random) const;
    float RollFootprintScale(FRandomStream& Random, EBuildingTier Tier) const;
    float GetBuildingHeight(FRandomStream& Random, EBuildingHeightClass HeightClass,
        EGrandCityDistrict District) const;
    void RecordBuildingStats(EBuildingTier Tier, EBuildingHeightClass HeightClass, EBuildingShape Shape);

    int32 MakeFeatureSeed(int32 BlockX, int32 BlockY, uint32 Salt) const;
    int32 ComputeConfigHash() const;
    bool CanEditBakedLayout() const;
    void MarkBakedDataDirty();
};
