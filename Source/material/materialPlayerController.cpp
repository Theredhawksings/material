#include "materialPlayerController.h"
#include "materialCharacter.h"

AmaterialPlayerController::AmaterialPlayerController()
{
	bShouldPerformFullTickWhenPaused = true;  // 일시정지 중에도 Tick 허용!
}

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
}

void AmaterialPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	FInputActionBinding& ClickBinding = InputComponent->BindAction("LeftMouseClick", IE_Pressed, this, &AmaterialPlayerController::OnMouseClick);
	ClickBinding.bExecuteWhenPaused = true;  

	FInputActionBinding& EscBinding = InputComponent->BindAction("EscapeKey", IE_Pressed, this, &AmaterialPlayerController::OnEscapeKey);
	EscBinding.bExecuteWhenPaused = true; 
}

void AmaterialPlayerController::OnMouseClick()
{
	AmaterialCharacter* MyPawn = Cast<AmaterialCharacter>(GetPawn()); 
	if (MyPawn)
	{
		MyPawn->OnLeftClick();
	}
}

void AmaterialPlayerController::OnEscapeKey()
{
	AmaterialCharacter* MyPawn = Cast<AmaterialCharacter>(GetPawn());  
	if (MyPawn)
	{
		MyPawn->OnEscapePressed();
	}
}