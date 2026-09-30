#include "Vehicles/GrandCityVehicleUpgradeComponent.h"

#include "Vehicles/GrandCityVehicle.h"
#include "Vehicles/GrandCityVehicleUpgradeStation.h"
#include "Quests/GrandCityQuestComponent.h"
#include "UI/GrandCityVehicleHudWidget.h"
#include "UI/GrandCityVehicleUpgradeWidget.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogGrandCityVehicleUpgrade, Log, All);

namespace GrandCityVehicleUpgrade
{
    /** Slack for latency between the client's area check and the server's. */
    constexpr float ServerRangeTolerance = 150.0f;
}

UGrandCityVehicleUpgradeComponent::UGrandCityVehicleUpgradeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UGrandCityVehicleUpgradeComponent::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocallyControlled())
    {
        HudWidget = CreateWidget<UGrandCityVehicleHudWidget>(
            GetOwningPlayerController(), UGrandCityVehicleHudWidget::StaticClass());
        if (HudWidget)
        {
            HudWidget->AddToViewport(5);
        }
    }
}

void UGrandCityVehicleUpgradeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CloseOffer();
    if (HudWidget)
    {
        HudWidget->RemoveFromParent();
        HudWidget = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}

APlayerController* UGrandCityVehicleUpgradeComponent::GetOwningPlayerController() const
{
    return Cast<APlayerController>(GetOwner());
}

bool UGrandCityVehicleUpgradeComponent::IsLocallyControlled() const
{
    const APlayerController* PlayerController = GetOwningPlayerController();
    return PlayerController && PlayerController->IsLocalController();
}

AGrandCityVehicle* UGrandCityVehicleUpgradeComponent::GetDrivenVehicle() const
{
    const APlayerController* PlayerController = GetOwningPlayerController();
    return PlayerController ? Cast<AGrandCityVehicle>(PlayerController->GetPawn()) : nullptr;
}

// --- Server -----------------------------------------------------------------------------------

AGrandCityVehicle* UGrandCityVehicleUpgradeComponent::GetVehicleAtStation(const AGrandCityVehicleUpgradeStation* Station) const
{
    AGrandCityVehicle* Vehicle = GetDrivenVehicle();
    const bool bInArea = Station && Vehicle
        && Station->IsLocationInTriggerArea(Vehicle->GetActorLocation(), GrandCityVehicleUpgrade::ServerRangeTolerance);
    return bInArea ? Vehicle : nullptr;
}

void UGrandCityVehicleUpgradeComponent::ServerUpgradeVehicle_Implementation(AGrandCityVehicleUpgradeStation* Station)
{
    AGrandCityVehicle* Vehicle = GetVehicleAtStation(Station);
    if (!Vehicle || !Station->ApplyUpgrade(Vehicle))
    {
        return;
    }

    UE_LOG(LogGrandCityVehicleUpgrade, Log, TEXT("%s installed %s on %s."),
        *GetNameSafe(GetOwner()), *Station->GetUpgradeName().ToString(), *GetNameSafe(Vehicle));
    ClientUpgradeInstalled(Station);
}

void UGrandCityVehicleUpgradeComponent::ServerPaintVehicle_Implementation(
    AGrandCityVehicleUpgradeStation* Station, int32 PaintIndex)
{
    AGrandCityVehicle* Vehicle = GetVehicleAtStation(Station);
    if (Vehicle && Station->ApplyPaint(Vehicle, PaintIndex))
    {
        UE_LOG(LogGrandCityVehicleUpgrade, Log, TEXT("%s painted %s %s."), *GetNameSafe(GetOwner()),
            *GetNameSafe(Vehicle), *Station->GetPaintPalette()[PaintIndex].Name.ToString());
    }
}

void UGrandCityVehicleUpgradeComponent::ClientUpgradeInstalled_Implementation(AGrandCityVehicleUpgradeStation* Station)
{
    if (!HudWidget)
    {
        return;
    }

    const FText Heading = FText::Format(NSLOCTEXT("GrandCityVehicleUpgrade", "Installed", "{0} INSTALLED"),
        Station ? Station->GetUpgradeName() : NSLOCTEXT("GrandCityVehicleUpgrade", "UpgradeFallback", "UPGRADE"));
    const FText Detail = PLATFORM_DESKTOP
        ? NSLOCTEXT("GrandCityVehicleUpgrade", "InstalledHintKey", "Hold SHIFT while driving to boost.")
        : NSLOCTEXT("GrandCityVehicleUpgrade", "InstalledHint", "Hold BOOST while driving to boost.");
    HudWidget->ShowMessage(Heading, Detail);
}

// --- Local interaction and UI -----------------------------------------------------------------

bool UGrandCityVehicleUpgradeComponent::UpdateLocalInteraction()
{
    CurrentStation.Reset();

    const AGrandCityVehicle* Vehicle = GetDrivenVehicle();
    UWorld* World = GetWorld();
    if (Vehicle && World)
    {
        const FVector Location = Vehicle->GetActorLocation();
        for (TActorIterator<AGrandCityVehicleUpgradeStation> It(World); It; ++It)
        {
            if (It->IsLocationInTriggerArea(Location))
            {
                CurrentStation = *It;
                break;
            }
        }
    }

    AGrandCityVehicleUpgradeStation* Station = CurrentStation.Get();
    if (DeclinedStation.IsValid() && DeclinedStation != CurrentStation)
    {
        DeclinedStation.Reset();
    }

    // Driving out of the station (or getting out of the car) closes its window.
    if (OfferWidget)
    {
        const AGrandCityVehicleUpgradeStation* Offered = OfferedStation.Get();
        if (!Offered || !Vehicle
            || !Offered->IsLocationInTriggerArea(Vehicle->GetActorLocation(), GrandCityVehicleUpgrade::ServerRangeTolerance * 0.5f))
        {
            CloseOffer();
        }
        else
        {
            // Follows the replicated vehicle state (e.g. the upgrade arriving from the server).
            OfferWidget->SetUpgradeAvailable(Offered->CanUpgrade(Vehicle));
        }
    }

    RefreshPrompt();
    return Station && !OfferWidget && Station != DeclinedStation.Get();
}

FText UGrandCityVehicleUpgradeComponent::GetLocalInteractionLabel() const
{
    return NSLOCTEXT("GrandCityVehicleUpgrade", "InteractWorkshop", "WORKSHOP");
}

void UGrandCityVehicleUpgradeComponent::RefreshPrompt()
{
    if (!HudWidget)
    {
        return;
    }

    const AGrandCityVehicleUpgradeStation* Station = CurrentStation.Get();
    const AGrandCityVehicle* Vehicle = GetDrivenVehicle();
    if (!Station || !Vehicle || OfferWidget || Station == DeclinedStation.Get())
    {
        HudWidget->SetPrompt(FText::GetEmpty());
        return;
    }

    HudWidget->SetPrompt(FText::Format(PLATFORM_DESKTOP
            ? NSLOCTEXT("GrandCityVehicleUpgrade", "PromptKey", "Press E to open the {0}")
            : NSLOCTEXT("GrandCityVehicleUpgrade", "PromptTouch", "Tap WORKSHOP to open the {0}"),
        Station->GetStationName()));
}

bool UGrandCityVehicleUpgradeComponent::TryLocalInteract()
{
    if (OfferWidget)
    {
        // The window is answered with its own buttons.
        return true;
    }

    // One window at a time: a follow-up quest offer can be open while driving.
    const UGrandCityQuestComponent* QuestComponent = GetOwner()->FindComponentByClass<UGrandCityQuestComponent>();
    if (QuestComponent && QuestComponent->IsOfferOpen())
    {
        return false;
    }

    if (!UpdateLocalInteraction())
    {
        return false;
    }

    ShowOffer(CurrentStation.Get());
    return true;
}

void UGrandCityVehicleUpgradeComponent::ShowOffer(AGrandCityVehicleUpgradeStation* Station)
{
    APlayerController* PlayerController = GetOwningPlayerController();
    if (!PlayerController || !Station)
    {
        return;
    }

    if (!OfferWidget)
    {
        OfferWidget = CreateWidget<UGrandCityVehicleUpgradeWidget>(PlayerController, UGrandCityVehicleUpgradeWidget::StaticClass());
        if (!OfferWidget)
        {
            return;
        }
        OfferWidget->OnUpgradeAccepted.BindUObject(this, &UGrandCityVehicleUpgradeComponent::HandleUpgradeAccepted);
        OfferWidget->OnPaintSelected.BindUObject(this, &UGrandCityVehicleUpgradeComponent::HandlePaintSelected);
        OfferWidget->OnClosed.BindUObject(this, &UGrandCityVehicleUpgradeComponent::HandleClosed);
    }

    OfferedStation = Station;
    const AGrandCityVehicle* Vehicle = GetDrivenVehicle();
    const FText Details = PLATFORM_DESKTOP
        ? NSLOCTEXT("GrandCityVehicleUpgrade", "NitroDetailsKey",
            "Hold SHIFT while driving for a short burst of extra speed. When the nitro runs out it refills slowly.")
        : NSLOCTEXT("GrandCityVehicleUpgrade", "NitroDetailsTouch",
            "Hold BOOST while driving for a short burst of extra speed. When the nitro runs out it refills slowly.");
    OfferWidget->SetUpgradeAvailable(Station->CanUpgrade(Vehicle));
    OfferWidget->SetStation(Station->GetStationName(), Station->GetUpgradeName(), Station->GetOfferQuestion(),
        Details, Station->GetPaintPalette());
    OfferWidget->SetSelectedPaint(Station->FindPaintIndex(Vehicle));
    if (!OfferWidget->IsInViewport())
    {
        OfferWidget->AddToViewport(20);
    }
    RefreshPrompt();

    // Desktop needs a cursor to click the buttons; touch screens already work. Checked by
    // platform, as in the quest offer window: PIE keeps the touch interface but uses the mouse.
    if (PLATFORM_DESKTOP)
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PlayerController->SetInputMode(InputMode);
        PlayerController->SetShowMouseCursor(true);
        bOfferChangedInputMode = true;
    }
}

void UGrandCityVehicleUpgradeComponent::CloseOffer()
{
    if (OfferWidget)
    {
        OfferWidget->RemoveFromParent();
        OfferWidget = nullptr;
    }
    OfferedStation.Reset();

    if (bOfferChangedInputMode)
    {
        bOfferChangedInputMode = false;
        if (APlayerController* PlayerController = GetOwningPlayerController())
        {
            PlayerController->SetInputMode(FInputModeGameOnly());
            PlayerController->SetShowMouseCursor(false);
        }
    }
}

void UGrandCityVehicleUpgradeComponent::HandleUpgradeAccepted()
{
    if (AGrandCityVehicleUpgradeStation* Station = OfferedStation.Get())
    {
        ServerUpgradeVehicle(Station);
    }
    CloseOffer();
}

void UGrandCityVehicleUpgradeComponent::HandlePaintSelected(int32 PaintIndex)
{
    // The window stays open so the player can try colours; the vehicle repaints once the
    // server's change replicates.
    if (AGrandCityVehicleUpgradeStation* Station = OfferedStation.Get())
    {
        ServerPaintVehicle(Station, PaintIndex);
    }
}

void UGrandCityVehicleUpgradeComponent::HandleClosed()
{
    DeclinedStation = OfferedStation;
    CloseOffer();
    RefreshPrompt();
}

bool UGrandCityVehicleUpgradeComponent::AcceptLocalOffer()
{
    return OfferWidget && OfferWidget->HandleAcceptKey();
}

bool UGrandCityVehicleUpgradeComponent::DeclineLocalOffer()
{
    if (!OfferWidget)
    {
        return false;
    }
    OfferWidget->HandleBackKey();
    return true;
}
