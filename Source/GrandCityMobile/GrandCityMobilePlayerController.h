#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GrandCityPlayerProfileComponent.h"
#include "Engine/TimerHandle.h"
#include "InputCoreTypes.h"
#include "GrandCityMobilePlayerController.generated.h"

class UGrandCityPlayerProfileComponent;
class UGrandCityMobileControlsWidget;
class SGrandCityVirtualJoystick;
class SVirtualJoystick;
class AGrandCityVehicle;
class UTouchInterface;
enum class EGrandCityVehicleControl : uint8;

UCLASS(Config=Game)
class GRANDCITYMOBILE_API AGrandCityMobilePlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AGrandCityMobilePlayerController();

    void LoadPersistentProfile(FGrandCityProfileComponentLoadResult Callback);
    void SavePersistentProfile(FGrandCityProfileComponentSaveResult Callback);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupInputComponent() override;
    virtual void FlushPressedKeys() override;
    virtual TSharedPtr<SVirtualJoystick> CreateVirtualJoystick() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void PawnLeavingGame() override;
    virtual void AutoManageActiveCameraTarget(AActor* SuggestedTarget) override;

public:
    UFUNCTION(Server, Reliable)
    void ServerEnterVehicle(AGrandCityVehicle* Vehicle);

    UFUNCTION(Server, Reliable)
    void ServerExitVehicle();

    UFUNCTION(Client, Reliable)
    void ClientInitializeSession();

    UFUNCTION(Client, Reliable)
    void ClientNotifySpawned();

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Session")
    bool bSessionInitialized = false;

    UPROPERTY(BlueprintReadOnly, Category="Grand City|Session")
    bool bSpawnConfirmed = false;

private:
    friend class SGrandCityVirtualJoystick;

    bool ShouldShowMobileControls() const;
    void HandleVirtualControlPressed(int32 PointerIndex, int32 ControlIndex);
    void HandleVirtualControlReleased(int32 PointerIndex);
    void HandleTouchStarted(ETouchIndex::Type FingerIndex, FVector Location);
    void HandleTouchMoved(ETouchIndex::Type FingerIndex, FVector Location);
    void HandleTouchEnded(ETouchIndex::Type FingerIndex, FVector Location);
    void ResetCameraTouch();
    void ResetTouchFeedback();
    void RefreshTouchFeedback();
    void HandleRunButtonPressed();
    void HandleJumpButtonPressed();
    void HandleJumpButtonReleased();
    void HandleJumpFeedbackExpired();
    void HandleCrouchButtonPressed();

    /** E key / ENTER / EXIT buttons: enter the nearby vehicle, or leave the current one. */
    void HandleInteractPressed();
    void HandleVehicleControlChanged(EGrandCityVehicleControl Control, bool bPressed);
    void ApplyTouchVehicleInput();
    void ResetTouchVehicleInput();
    /** Local only. Tracks the nearest free vehicle and switches the UI between on-foot and driving. */
    void UpdateVehicleInteraction();
    void SetVehicleControlsActive(bool bActive);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grand City|Persistence", meta=(AllowPrivateAccess="true"))
    TObjectPtr<UGrandCityPlayerProfileComponent> PlayerProfileComponent;

    UPROPERTY(Transient)
    TObjectPtr<UGrandCityMobileControlsWidget> MobileControlsWidget;

    /** Degrees of controller input applied per pixel of touch movement. */
    UPROPERTY(EditDefaultsOnly, Config, Category="Grand City|Mobile Controls", meta=(ClampMin="0.001", ClampMax="1.0"))
    float TouchLookSensitivity = 0.12f;

    FVector2D LastCameraTouchPosition = FVector2D::ZeroVector;
    TEnumAsByte<ETouchIndex::Type> CameraTouchFinger = ETouchIndex::MAX_TOUCHES;
    TMap<int32, int32> ActiveVirtualControls;
    /** Pointer IDs ordered from oldest to newest; INDEX_NONE represents the camera touch. */
    TArray<int32> TouchInteractionOrder;
    FTimerHandle MobileJumpReleaseTimer;
    FTimerHandle JumpFeedbackTimer;
    bool bCameraTouchActive = false;
    bool bJumpFeedbackActive = false;

    /** The on-foot touch joystick, restored after leaving a vehicle. */
    UPROPERTY(Transient)
    TObjectPtr<UTouchInterface> OnFootTouchInterface;

    TWeakObjectPtr<AGrandCityVehicle> NearbyVehicle;
    FTimerHandle VehicleInteractionTimer;
    bool bVehicleControlsActive = false;
    bool bTouchForwardHeld = false;
    bool bTouchReverseHeld = false;
    bool bTouchBrakeHeld = false;
    bool bTouchSteerLeftHeld = false;
    bool bTouchSteerRightHeld = false;
};
