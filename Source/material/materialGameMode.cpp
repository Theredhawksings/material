#include "materialGameMode.h"
#include "materialCharacter.h"
#include "materialPlayerController.h"

AmaterialGameMode::AmaterialGameMode()
{
    DefaultPawnClass = AmaterialCharacter::StaticClass();
    PlayerControllerClass = AmaterialPlayerController::StaticClass();
    
    UE_LOG(LogTemp, Warning, TEXT("GameMode Constructor - PlayerControllerClass: %s"), 
           PlayerControllerClass ? *PlayerControllerClass->GetName() : TEXT("NULL"));
}