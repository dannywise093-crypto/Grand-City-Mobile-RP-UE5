#include "GrandCityPlayerController.h"

void AGrandCityPlayerController::BeginPlay()
{
    Super::BeginPlay();

    bShowMouseCursor = false;
    SetInputMode(FInputModeGameOnly());
}
