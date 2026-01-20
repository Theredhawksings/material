// Copyright Epic Games, Inc. All Rights Reserved.

#include "materialPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/Input/SVirtualJoystick.h"

AmaterialPlayerController::AmaterialPlayerController()
{
	// Input Mapping Context 로드
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MappingContextAsset(
		TEXT("/Game/Input/IMC_Default")
	);
	
	if (MappingContextAsset.Succeeded())
	{
		DefaultMappingContexts.Add(MappingContextAsset.Object);
		UE_LOG(LogTemp, Log, TEXT("PlayerController: IMC_Default loaded and added to array"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerController: Failed to load IMC_Default from /Game/Input/IMC_Default"));
	}
}

void AmaterialPlayerController::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("PlayerController BeginPlay called"));

	// only spawn touch controls on local player controllers
	if (SVirtualJoystick::ShouldDisplayTouchInterface() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogTemp, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AmaterialPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UE_LOG(LogTemp, Log, TEXT("PlayerController SetupInputComponent called"));

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			UE_LOG(LogTemp, Log, TEXT("Adding %d default mapping contexts"), DefaultMappingContexts.Num());
			
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				if (CurrentContext)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
					UE_LOG(LogTemp, Log, TEXT("Added mapping context: %s"), *CurrentContext->GetName());
				}
			}

			// only add these IMCs if we're not using mobile touch input
			if (!SVirtualJoystick::ShouldDisplayTouchInterface())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					if (CurrentContext)
					{
						Subsystem->AddMappingContext(CurrentContext, 0);
					}
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get EnhancedInputLocalPlayerSubsystem"));
		}
	}
}