#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrandCityVehicleUpgradeStation.generated.h"

class AGrandCityVehicle;
class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** One colour of a workshop's paint palette. */
USTRUCT(BlueprintType)
struct FGrandCityPaintOption
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Paint")
    FText Name;

    /** Linear colour fed to the body material's PaintColor parameter. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Paint")
    FLinearColor Color = FLinearColor::White;
};

/**
 * Place in the level as a vehicle workshop. A player driving a vehicle into TriggerArea can
 * interact (E key / WORKSHOP button) to open its menu: change the paint colour or buy the
 * Nitro Boost upgrade. Move/scale the actor and TriggerArea to change where it is.
 */
UCLASS()
class GRANDCITYMOBILE_API AGrandCityVehicleUpgradeStation : public AActor
{
    GENERATED_BODY()

public:
    AGrandCityVehicleUpgradeStation();

    /** True when WorldLocation lies inside the trigger box, grown by Tolerance on every side. */
    bool IsLocationInTriggerArea(const FVector& WorldLocation, float Tolerance = 0.0f) const;

    /** The vehicle does not have the upgrade yet (ignores location). */
    bool CanUpgrade(const AGrandCityVehicle* Vehicle) const;

    /** Server only. Installs the upgrade; returns false when the vehicle already had it. */
    bool ApplyUpgrade(AGrandCityVehicle* Vehicle) const;

    /** Server only. Paints the vehicle with PaintPalette[PaintIndex]; false for a bad index. */
    bool ApplyPaint(AGrandCityVehicle* Vehicle, int32 PaintIndex) const;

    /** Palette entry the vehicle is painted with, or INDEX_NONE (factory paint / other colour). */
    int32 FindPaintIndex(const AGrandCityVehicle* Vehicle) const;

    const FText& GetStationName() const { return StationName; }
    const FText& GetUpgradeName() const { return UpgradeName; }
    const FText& GetOfferQuestion() const { return OfferQuestion; }
    const TArray<FGrandCityPaintOption>& GetPaintPalette() const { return PaintPalette; }

    /** Title of the workshop menu and the text floating over the beacon. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
    FText StationName = NSLOCTEXT("GrandCityVehicleUpgrade", "StationName", "VEHICLE WORKSHOP");

    /** Name of the upgrade sold here, shown on its menu button and window. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
    FText UpgradeName = NSLOCTEXT("GrandCityVehicleUpgrade", "NitroName", "NITRO BOOST");

    /** Question asked in the upgrade window. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
    FText OfferQuestion = NSLOCTEXT("GrandCityVehicleUpgrade", "NitroQuestion",
        "Do you want to upgrade your vehicle with Nitro Boost?");

    /** Colours offered in the Change Color window, in display order (3 per row). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Paint")
    TArray<FGrandCityPaintOption> PaintPalette;

    /** Height of the green beacon tube over the trigger area. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Beacon", meta=(ClampMin="10.0", Units="cm"))
    float BeaconHeight = 300.0f;

    /** 0 = invisible, 1 = solid. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Beacon", meta=(ClampMin="0.0", ClampMax="1.0"))
    float BeaconOpacity = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|Beacon")
    FLinearColor BeaconColor = FLinearColor(0.1f, 1.0f, 0.25f);

protected:
    virtual void OnConstruction(const FTransform& Transform) override;

    /** Fits the beacon tube to the trigger area. */
    void UpdateBeacon();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Upgrade")
    TObjectPtr<USceneComponent> Root;

    /** Vehicles whose centre is inside this box can use the workshop. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Upgrade")
    TObjectPtr<UBoxComponent> TriggerArea;

    /** Translucent green tube filling the trigger area; sized by UpdateBeacon. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Upgrade")
    TObjectPtr<UStaticMeshComponent> Marker;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Upgrade")
    TObjectPtr<UTextRenderComponent> Label;
};
