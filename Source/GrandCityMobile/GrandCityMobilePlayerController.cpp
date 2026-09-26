#include "GrandCityMobilePlayerController.h"

#include "UI/GrandCityMobileControlsWidget.h"
#include "GrandCityMobileCharacter.h"
#include "GrandCityPlayerProfileComponent.h"
#include "Vehicles/GrandCityVehicle.h"
#include "Components/InputComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/TouchInterface.h"
#include "TimerManager.h"
#include "Widgets/Input/SVirtualJoystick.h"

DEFINE_LOG_CATEGORY_STATIC(LogGrandCityMobileControls, Log, All);

class SGrandCityVirtualJoystick final : public SVirtualJoystick
{
public:
    SLATE_BEGIN_ARGS(SGrandCityVirtualJoystick)
    {
    }
        SLATE_ARGUMENT(TWeakObjectPtr<AGrandCityMobilePlayerController>, OwningController)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        OwningController = InArgs._OwningController;
        SVirtualJoystick::Construct(SVirtualJoystick::FArguments());
    }

    virtual FReply OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& Event) override
    {
        const FReply Reply = SVirtualJoystick::OnTouchStarted(MyGeometry, Event);
        const int32 PointerIndex = Event.GetPointerIndex();

        for (int32 ControlIndex = 0; ControlIndex < Controls.Num(); ++ControlIndex)
        {
            if (Controls[ControlIndex].CapturedPointerIndex == PointerIndex)
            {
                if (AGrandCityMobilePlayerController* Controller = OwningController.Get())
                {
                    Controller->HandleVirtualControlPressed(
                        PointerIndex, ResolveFeedbackControlIndex(ControlIndex));
                }
                break;
            }
        }

        return Reply;
    }

    virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& Event) override
    {
        const int32 PointerIndex = Event.GetPointerIndex();
        bool bWasVirtualControl = false;

        for (const FControlData& Control : Controls)
        {
            if (Control.CapturedPointerIndex == PointerIndex)
            {
                bWasVirtualControl = true;
                break;
            }
        }

        const FReply Reply = SVirtualJoystick::OnTouchEnded(MyGeometry, Event);
        if (bWasVirtualControl)
        {
            if (AGrandCityMobilePlayerController* Controller = OwningController.Get())
            {
                Controller->HandleVirtualControlReleased(PointerIndex);
            }
        }

        return Reply;
    }

private:
    int32 ResolveFeedbackControlIndex(int32 ControlIndex) const
    {
        if (!Controls.IsValidIndex(ControlIndex))
        {
            return INDEX_NONE;
        }

        const FControlInfo& Info = Controls[ControlIndex].Info;
        if (Info.MainInputKey == EKeys::Gamepad_LeftX || Info.AltInputKey == EKeys::Gamepad_LeftY)
        {
            return 0;
        }
        if (Info.MainInputKey == EKeys::Gamepad_RightX || Info.AltInputKey == EKeys::Gamepad_RightY)
        {
            return 1;
        }

        // The built-in DefaultVirtualJoysticks asset stores left then right.
        return ControlIndex;
    }

    TWeakObjectPtr<AGrandCityMobilePlayerController> OwningController;
};

AGrandCityMobilePlayerController::AGrandCityMobilePlayerController()
{
    bReplicates = true;
    PlayerProfileComponent = CreateDefaultSubobject<UGrandCityPlayerProfileComponent>(TEXT("PlayerProfileComponent"));
}

void AGrandCityMobilePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocalController())
    {
        ClientInitializeSession();
    }

    if (IsLocalPlayerController() && ShouldShowMobileControls())
    {
        MobileControlsWidget = CreateWidget<UGrandCityMobileControlsWidget>(
            this, UGrandCityMobileControlsWidget::StaticClass());
        if (MobileControlsWidget)
        {
            MobileControlsWidget->OnRunPressed.BindUObject(
                this, &AGrandCityMobilePlayerController::HandleRunButtonPressed);
            MobileControlsWidget->OnJumpPressed.BindUObject(
                this, &AGrandCityMobilePlayerController::HandleJumpButtonPressed);
            MobileControlsWidget->OnCrouchPressed.BindUObject(
                this, &AGrandCityMobilePlayerController::HandleCrouchButtonPressed);
            MobileControlsWidget->OnEnterVehiclePressed.BindUObject(
                this, &AGrandCityMobilePlayerController::HandleInteractPressed);
            MobileControlsWidget->OnExitVehiclePressed.BindUObject(
                this, &AGrandCityMobilePlayerController::HandleInteractPressed);
            MobileControlsWidget->OnVehicleControlChanged.BindUObject(
                this, &AGrandCityMobilePlayerController::HandleVehicleControlChanged);
            // The built-in virtual joystick is a full-screen Slate widget at
            // viewport Z-order 0. Keep the action buttons in the same overlay
            // above it so their hit-test path receives touch events.
            MobileControlsWidget->AddToViewport(10);
            RefreshTouchFeedback();
            UE_LOG(LogTemp, Display,
                TEXT("Grand City mobile controls enabled: left joystick, RUN/JUMP/CROUCH buttons, and free-area camera swipe."));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Grand City could not create the mobile controls widget."));
        }
    }

    if (IsLocalPlayerController())
    {
        GetWorldTimerManager().SetTimer(
            VehicleInteractionTimer,
            this,
            &AGrandCityMobilePlayerController::UpdateVehicleInteraction,
            0.1f,
            true);
    }
}

void AGrandCityMobilePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(VehicleInteractionTimer);
    HandleJumpButtonReleased();
    ResetTouchFeedback();

    if (MobileControlsWidget)
    {
        MobileControlsWidget->RemoveFromParent();
        MobileControlsWidget = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}

void AGrandCityMobilePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (!InputComponent)
    {
        return;
    }

    InputComponent->BindTouch(IE_Pressed, this, &AGrandCityMobilePlayerController::HandleTouchStarted);
    InputComponent->BindTouch(IE_Repeat, this, &AGrandCityMobilePlayerController::HandleTouchMoved);
    InputComponent->BindTouch(IE_Released, this, &AGrandCityMobilePlayerController::HandleTouchEnded);
    // Bound on the controller so the same key enters (on foot) and exits (driving).
    InputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &AGrandCityMobilePlayerController::HandleInteractPressed);
}

void AGrandCityMobilePlayerController::FlushPressedKeys()
{
    HandleJumpButtonReleased();
    ResetTouchFeedback();
    ResetTouchVehicleInput();
    Super::FlushPressedKeys();
}

TSharedPtr<SVirtualJoystick> AGrandCityMobilePlayerController::CreateVirtualJoystick()
{
    return SNew(SGrandCityVirtualJoystick)
        .OwningController(TWeakObjectPtr<AGrandCityMobilePlayerController>(this));
}

void AGrandCityMobilePlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (IsLocalController())
    {
        ClientNotifySpawned();
    }
}

void AGrandCityMobilePlayerController::OnUnPossess()
{
    HandleJumpButtonReleased();
    ResetTouchFeedback();
    bSpawnConfirmed = false;
    Super::OnUnPossess();
}

bool AGrandCityMobilePlayerController::ShouldShowMobileControls() const
{
    return SVirtualJoystick::ShouldDisplayTouchInterface();
}

void AGrandCityMobilePlayerController::HandleVirtualControlPressed(int32 PointerIndex, int32 ControlIndex)
{
    if (ControlIndex == INDEX_NONE)
    {
        return;
    }

    ActiveVirtualControls.Add(PointerIndex, ControlIndex);
    TouchInteractionOrder.Remove(PointerIndex);
    TouchInteractionOrder.Add(PointerIndex);
    RefreshTouchFeedback();
}

void AGrandCityMobilePlayerController::HandleVirtualControlReleased(int32 PointerIndex)
{
    ActiveVirtualControls.Remove(PointerIndex);
    TouchInteractionOrder.Remove(PointerIndex);
    RefreshTouchFeedback();
}

void AGrandCityMobilePlayerController::HandleTouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
    // The virtual joysticks consume touches in their own regions. A touch that
    // reaches this binding began in free viewport space and controls the camera.
    if (!ShouldShowMobileControls() || bCameraTouchActive)
    {
        return;
    }

    bCameraTouchActive = true;
    CameraTouchFinger = FingerIndex;
    LastCameraTouchPosition = FVector2D(Location.X, Location.Y);
    TouchInteractionOrder.Remove(INDEX_NONE);
    TouchInteractionOrder.Add(INDEX_NONE);
    RefreshTouchFeedback();
}

void AGrandCityMobilePlayerController::HandleTouchMoved(ETouchIndex::Type FingerIndex, FVector Location)
{
    if (!bCameraTouchActive || FingerIndex != CameraTouchFinger)
    {
        return;
    }

    const FVector2D CurrentPosition(Location.X, Location.Y);
    const FVector2D TouchDelta = (CurrentPosition - LastCameraTouchPosition).GetClampedToMaxSize(100.0f);
    LastCameraTouchPosition = CurrentPosition;

    AddYawInput(TouchDelta.X * TouchLookSensitivity);
    AddPitchInput(-TouchDelta.Y * TouchLookSensitivity);
}

void AGrandCityMobilePlayerController::HandleTouchEnded(ETouchIndex::Type FingerIndex, FVector Location)
{
    if (bCameraTouchActive && FingerIndex == CameraTouchFinger)
    {
        ResetCameraTouch();
    }
}

void AGrandCityMobilePlayerController::ResetCameraTouch()
{
    bCameraTouchActive = false;
    CameraTouchFinger = ETouchIndex::MAX_TOUCHES;
    LastCameraTouchPosition = FVector2D::ZeroVector;
    TouchInteractionOrder.Remove(INDEX_NONE);
    RefreshTouchFeedback();
}

void AGrandCityMobilePlayerController::ResetTouchFeedback()
{
    GetWorldTimerManager().ClearTimer(MobileJumpReleaseTimer);
    GetWorldTimerManager().ClearTimer(JumpFeedbackTimer);
    bJumpFeedbackActive = false;
    bCameraTouchActive = false;
    CameraTouchFinger = ETouchIndex::MAX_TOUCHES;
    LastCameraTouchPosition = FVector2D::ZeroVector;
    ActiveVirtualControls.Reset();
    TouchInteractionOrder.Reset();

    if (MobileControlsWidget)
    {
        MobileControlsWidget->ClearFeedback();
    }
}

void AGrandCityMobilePlayerController::RefreshTouchFeedback()
{
    if (!MobileControlsWidget)
    {
        return;
    }

    if (bVehicleControlsActive)
    {
        // Walk/run/camera hints describe on-foot controls only.
        MobileControlsWidget->ClearFeedback();
        return;
    }

    if (bJumpFeedbackActive)
    {
        MobileControlsWidget->ShowFeedback(
            NSLOCTEXT("GrandCityMobileControls", "Jump", "JUMP"));
        return;
    }

    if (const AGrandCityMobileCharacter* MobileCharacter =
        Cast<AGrandCityMobileCharacter>(GetPawn()))
    {
        if (MobileCharacter->WantsToCrouch())
        {
            MobileControlsWidget->ShowFeedback(
                NSLOCTEXT("GrandCityMobileControls", "Crouch", "CROUCH"));
            return;
        }

        if (MobileCharacter->IsSprinting())
        {
            MobileControlsWidget->ShowFeedback(
                NSLOCTEXT("GrandCityMobileControls", "Run", "RUN"));
            return;
        }
    }

    for (int32 OrderIndex = TouchInteractionOrder.Num() - 1; OrderIndex >= 0; --OrderIndex)
    {
        const int32 PointerIndex = TouchInteractionOrder[OrderIndex];
        if (PointerIndex == INDEX_NONE)
        {
            if (bCameraTouchActive)
            {
                MobileControlsWidget->ShowFeedback(
                    NSLOCTEXT("GrandCityMobileControls", "RotateCamera", "ROTATE CAMERA"));
                return;
            }
            continue;
        }

        const int32* ControlIndex = ActiveVirtualControls.Find(PointerIndex);
        if (!ControlIndex)
        {
            continue;
        }

        if (*ControlIndex == 0)
        {
            MobileControlsWidget->ShowFeedback(
                NSLOCTEXT("GrandCityMobileControls", "Walk", "WALK"));
        }
        else
        {
            MobileControlsWidget->ShowFeedback(
                NSLOCTEXT("GrandCityMobileControls", "TouchControl", "TOUCH CONTROL"));
        }
        return;
    }

    MobileControlsWidget->ClearFeedback();
}

void AGrandCityMobilePlayerController::HandleInteractPressed()
{
    if (Cast<AGrandCityVehicle>(GetPawn()))
    {
        ServerExitVehicle();
        return;
    }

    // Refresh first so a key press right after walking up still finds the car.
    UpdateVehicleInteraction();
    if (AGrandCityVehicle* Vehicle = NearbyVehicle.Get())
    {
        ServerEnterVehicle(Vehicle);
    }
}

void AGrandCityMobilePlayerController::UpdateVehicleInteraction()
{
    SetVehicleControlsActive(Cast<AGrandCityVehicle>(GetPawn()) != nullptr);

    AGrandCityVehicle* BestVehicle = nullptr;
    const AGrandCityMobileCharacter* MobileCharacter = Cast<AGrandCityMobileCharacter>(GetPawn());
    if (MobileCharacter && !MobileCharacter->GetOccupiedVehicle() && GetWorld())
    {
        const FVector CharacterLocation = MobileCharacter->GetActorLocation();
        float BestDistance = TNumericLimits<float>::Max();
        for (TActorIterator<AGrandCityVehicle> It(GetWorld()); It; ++It)
        {
            AGrandCityVehicle* Vehicle = *It;
            if (Vehicle->HasDriver())
            {
                continue;
            }

            const float Distance = Vehicle->GetDistanceToVehicle(CharacterLocation);
            if (Distance <= Vehicle->EnterRange && Distance < BestDistance)
            {
                BestDistance = Distance;
                BestVehicle = Vehicle;
            }
        }
    }

    NearbyVehicle = BestVehicle;
    if (MobileControlsWidget)
    {
        MobileControlsWidget->SetEnterVehicleAvailable(BestVehicle != nullptr);
    }
}

void AGrandCityMobilePlayerController::SetVehicleControlsActive(bool bActive)
{
    if (bVehicleControlsActive == bActive)
    {
        return;
    }

    bVehicleControlsActive = bActive;
    ResetTouchVehicleInput();

    if (!MobileControlsWidget)
    {
        return;
    }

    // The left joystick would sit under the steering buttons, so it is removed
    // while driving and restored when the player is back on foot.
    if (bActive)
    {
        if (CurrentTouchInterface)
        {
            OnFootTouchInterface = CurrentTouchInterface;
        }
        ActivateTouchInterface(nullptr);
    }
    else
    {
        ActivateTouchInterface(OnFootTouchInterface);
    }

    MobileControlsWidget->SetVehicleMode(bActive);
    ResetTouchFeedback();
    RefreshTouchFeedback();
}

void AGrandCityMobilePlayerController::HandleVehicleControlChanged(EGrandCityVehicleControl Control, bool bPressed)
{
    switch (Control)
    {
    case EGrandCityVehicleControl::Forward:
        bTouchForwardHeld = bPressed;
        break;
    case EGrandCityVehicleControl::Reverse:
        bTouchReverseHeld = bPressed;
        break;
    case EGrandCityVehicleControl::Brake:
        bTouchBrakeHeld = bPressed;
        break;
    case EGrandCityVehicleControl::SteerLeft:
        bTouchSteerLeftHeld = bPressed;
        break;
    case EGrandCityVehicleControl::SteerRight:
        bTouchSteerRightHeld = bPressed;
        break;
    }

    ApplyTouchVehicleInput();
}

void AGrandCityMobilePlayerController::ApplyTouchVehicleInput()
{
    if (AGrandCityVehicle* Vehicle = Cast<AGrandCityVehicle>(GetPawn()))
    {
        Vehicle->SetTouchThrottle((bTouchForwardHeld ? 1.0f : 0.0f) - (bTouchReverseHeld ? 1.0f : 0.0f));
        Vehicle->SetTouchSteering((bTouchSteerRightHeld ? 1.0f : 0.0f) - (bTouchSteerLeftHeld ? 1.0f : 0.0f));
        Vehicle->SetTouchBrake(bTouchBrakeHeld);
    }
}

void AGrandCityMobilePlayerController::ResetTouchVehicleInput()
{
    bTouchForwardHeld = false;
    bTouchReverseHeld = false;
    bTouchBrakeHeld = false;
    bTouchSteerLeftHeld = false;
    bTouchSteerRightHeld = false;
    ApplyTouchVehicleInput();
}

void AGrandCityMobilePlayerController::ServerEnterVehicle_Implementation(AGrandCityVehicle* Vehicle)
{
    AGrandCityMobileCharacter* MobileCharacter = Cast<AGrandCityMobileCharacter>(GetPawn());
    if (!MobileCharacter || !Vehicle || Vehicle->HasDriver() || MobileCharacter->GetOccupiedVehicle())
    {
        return;
    }

    // Allow some slack for latency between the client's range check and the server.
    constexpr float EnterRangeTolerance = 150.0f;
    if (Vehicle->GetDistanceToVehicle(MobileCharacter->GetActorLocation()) > Vehicle->EnterRange + EnterRangeTolerance)
    {
        return;
    }

    Vehicle->SetDriver(MobileCharacter);
    MobileCharacter->EnterVehicle(Vehicle);
    Possess(Vehicle);
    UE_LOG(LogGrandCityMobileControls, Log, TEXT("%s entered %s."), *GetNameSafe(this), *GetNameSafe(Vehicle));
}

void AGrandCityMobilePlayerController::ServerExitVehicle_Implementation()
{
    AGrandCityVehicle* Vehicle = Cast<AGrandCityVehicle>(GetPawn());
    AGrandCityMobileCharacter* MobileCharacter = Vehicle ? Vehicle->GetDriver() : nullptr;
    if (!MobileCharacter)
    {
        return;
    }

    FVector ExitLocation;
    FRotator ExitRotation;
    if (!Vehicle->FindExitTransform(MobileCharacter, ExitLocation, ExitRotation))
    {
        // Boxed in: stay in the car rather than spawning inside a wall.
        UE_LOG(LogGrandCityMobileControls, Warning, TEXT("No free exit spot around %s."), *GetNameSafe(Vehicle));
        return;
    }

    Vehicle->SetDriver(nullptr);
    MobileCharacter->ExitVehicle(ExitLocation, ExitRotation);
    Possess(MobileCharacter);
    // Put the on-foot camera behind the character, looking the way the car faced.
    ClientSetRotation(FRotator(-10.0f, ExitRotation.Yaw, 0.0f));
    UE_LOG(LogGrandCityMobileControls, Log, TEXT("%s exited %s."), *GetNameSafe(this), *GetNameSafe(Vehicle));
}

void AGrandCityMobilePlayerController::PawnLeavingGame()
{
    // Leaving while driving: keep the vehicle in the world and remove the player's body.
    if (AGrandCityVehicle* Vehicle = Cast<AGrandCityVehicle>(GetPawn()))
    {
        AGrandCityMobileCharacter* Driver = Vehicle->GetDriver();
        Vehicle->SetDriver(nullptr);
        UnPossess();
        if (Driver)
        {
            Driver->Destroy();
        }
        return;
    }

    Super::PawnLeavingGame();
}

void AGrandCityMobilePlayerController::AutoManageActiveCameraTarget(AActor* SuggestedTarget)
{
    // Blend the camera between the character and the vehicle instead of cutting.
    AActor* CurrentViewTarget = GetViewTarget();
    const bool bVehicleSwap = bAutoManageActiveCameraTarget
        && SuggestedTarget
        && CurrentViewTarget
        && CurrentViewTarget != SuggestedTarget
        && (Cast<AGrandCityVehicle>(SuggestedTarget) || Cast<AGrandCityVehicle>(CurrentViewTarget));
    if (bVehicleSwap)
    {
        SetViewTargetWithBlend(SuggestedTarget, 0.45f, VTBlend_EaseInOut, 2.0f);
        return;
    }

    Super::AutoManageActiveCameraTarget(SuggestedTarget);
}

void AGrandCityMobilePlayerController::HandleRunButtonPressed()
{
    if (AGrandCityMobileCharacter* MobileCharacter = Cast<AGrandCityMobileCharacter>(GetPawn()))
    {
        MobileCharacter->ToggleSprint();
        RefreshTouchFeedback();
        UE_LOG(LogGrandCityMobileControls, Verbose,
            TEXT("RUN toggled %s for %s."),
            MobileCharacter->IsSprinting() ? TEXT("on") : TEXT("off"),
            *GetNameSafe(MobileCharacter));
    }
    else
    {
        UE_LOG(LogGrandCityMobileControls, Warning,
            TEXT("RUN ignored because the controller has no Grand City character pawn."));
    }
}

void AGrandCityMobilePlayerController::HandleJumpButtonPressed()
{
    if (AGrandCityMobileCharacter* MobileCharacter = Cast<AGrandCityMobileCharacter>(GetPawn()))
    {
        MobileCharacter->Jump();
        bJumpFeedbackActive = true;
        RefreshTouchFeedback();

        // Mobile jump is a discrete action. Releasing it on a short timer avoids
        // depending on a captured touch-up event while still allowing the
        // character movement tick to consume the jump request.
        GetWorldTimerManager().SetTimer(
            MobileJumpReleaseTimer,
            this,
            &AGrandCityMobilePlayerController::HandleJumpButtonReleased,
            0.12f,
            false);
        GetWorldTimerManager().SetTimer(
            JumpFeedbackTimer,
            this,
            &AGrandCityMobilePlayerController::HandleJumpFeedbackExpired,
            0.60f,
            false);
        UE_LOG(LogGrandCityMobileControls, Verbose,
            TEXT("JUMP pressed for %s."), *GetNameSafe(MobileCharacter));
    }
    else
    {
        UE_LOG(LogGrandCityMobileControls, Warning,
            TEXT("JUMP ignored because the controller has no Grand City character pawn."));
    }
}

void AGrandCityMobilePlayerController::HandleJumpButtonReleased()
{
    GetWorldTimerManager().ClearTimer(MobileJumpReleaseTimer);
    if (AGrandCityMobileCharacter* MobileCharacter = Cast<AGrandCityMobileCharacter>(GetPawn()))
    {
        MobileCharacter->StopJumping();
        UE_LOG(LogGrandCityMobileControls, Verbose,
            TEXT("JUMP released for %s."), *GetNameSafe(MobileCharacter));
    }
}

void AGrandCityMobilePlayerController::HandleJumpFeedbackExpired()
{
    bJumpFeedbackActive = false;
    RefreshTouchFeedback();
}

void AGrandCityMobilePlayerController::HandleCrouchButtonPressed()
{
    if (AGrandCityMobileCharacter* MobileCharacter = Cast<AGrandCityMobileCharacter>(GetPawn()))
    {
        MobileCharacter->ToggleCrouch();
        RefreshTouchFeedback();
        UE_LOG(LogGrandCityMobileControls, Verbose,
            TEXT("CROUCH pressed for %s."), *GetNameSafe(MobileCharacter));
    }
    else
    {
        UE_LOG(LogGrandCityMobileControls, Warning,
            TEXT("CROUCH ignored because the controller has no Grand City character pawn."));
    }
}

void AGrandCityMobilePlayerController::LoadPersistentProfile(FGrandCityProfileComponentLoadResult Callback)
{
    if (!HasAuthority() || !PlayerProfileComponent)
    {
        Callback(false);
        return;
    }

    PlayerProfileComponent->LoadProfile(MoveTemp(Callback));
}

void AGrandCityMobilePlayerController::SavePersistentProfile(FGrandCityProfileComponentSaveResult Callback)
{
    if (!HasAuthority() || !PlayerProfileComponent)
    {
        Callback(false);
        return;
    }

    PlayerProfileComponent->SaveProfile(MoveTemp(Callback));
}

void AGrandCityMobilePlayerController::ClientInitializeSession_Implementation()
{
    bSessionInitialized = true;
}

void AGrandCityMobilePlayerController::ClientNotifySpawned_Implementation()
{
    bSpawnConfirmed = true;
}
