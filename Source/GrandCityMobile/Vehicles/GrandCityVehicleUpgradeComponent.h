#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrandCityVehicleUpgradeComponent.generated.h"

class AGrandCityVehicle;
class AGrandCityVehicleUpgradeStation;
class APlayerController;
class UGrandCityVehicleHudWidget;
class UGrandCityVehicleUpgradeWidget;

/**
 * Per-player side of the vehicle workshops (upgrade stations), owned by the player controller.
 * The owning client finds the workshop its car is parked in, shows the prompt and the workshop
 * window; the server checks each request and paints or upgrades the vehicle.
 */
UCLASS(ClassGroup=(GrandCity), meta=(BlueprintSpawnableComponent))
class GRANDCITYMOBILE_API UGrandCityVehicleUpgradeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGrandCityVehicleUpgradeComponent();

    // --- Local (owning client) ---------------------------------------------------------------

    /** Polled by the controller. Returns true while the interact button should open a workshop. */
    bool UpdateLocalInteraction();
    FText GetLocalInteractionLabel() const;
    /** Handles an interact press while driving. Returns false when no workshop is in reach. */
    bool TryLocalInteract();
    /**
     * Y / U keys on the open workshop window: Y accepts the upgrade offer, U goes back (or
     * closes from the menu). Returns false when no window is open (or Y does not apply).
     */
    bool AcceptLocalOffer();
    bool DeclineLocalOffer();
    bool IsOfferOpen() const { return OfferWidget != nullptr; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(Server, Reliable)
    void ServerUpgradeVehicle(AGrandCityVehicleUpgradeStation* Station);

    UFUNCTION(Server, Reliable)
    void ServerPaintVehicle(AGrandCityVehicleUpgradeStation* Station, int32 PaintIndex);

    UFUNCTION(Client, Reliable)
    void ClientUpgradeInstalled(AGrandCityVehicleUpgradeStation* Station);

private:
    APlayerController* GetOwningPlayerController() const;
    bool IsLocallyControlled() const;
    AGrandCityVehicle* GetDrivenVehicle() const;
    /** Server only. The driven vehicle, if it is inside Station (with latency slack). */
    AGrandCityVehicle* GetVehicleAtStation(const AGrandCityVehicleUpgradeStation* Station) const;

    void ShowOffer(AGrandCityVehicleUpgradeStation* Station);
    void CloseOffer();
    void HandleUpgradeAccepted();
    void HandlePaintSelected(int32 PaintIndex);
    void HandleClosed();
    void RefreshPrompt();

    UPROPERTY(Transient)
    TObjectPtr<UGrandCityVehicleUpgradeWidget> OfferWidget;

    UPROPERTY(Transient)
    TObjectPtr<UGrandCityVehicleHudWidget> HudWidget;

    /** Station the driven car is parked in. */
    TWeakObjectPtr<AGrandCityVehicleUpgradeStation> CurrentStation;
    TWeakObjectPtr<AGrandCityVehicleUpgradeStation> OfferedStation;
    /**
     * Station whose window was closed with CLOSE. It stays quiet until the car leaves it, so E
     * goes back to leaving the vehicle instead of reopening the window.
     */
    TWeakObjectPtr<AGrandCityVehicleUpgradeStation> DeclinedStation;
    bool bOfferChangedInputMode = false;
};
