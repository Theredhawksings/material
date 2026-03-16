#include "materialPlayerController.h"
#include "materialCharacter.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

void AmaterialPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	
	if (GetLocalPlayer() && GetLocalPlayer()->ViewportClient)
	{
		GetLocalPlayer()->ViewportClient->SetMouseLockMode(EMouseLockMode::DoNotLock);
		GetLocalPlayer()->ViewportClient->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
	}
}