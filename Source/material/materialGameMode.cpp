#include "materialGameMode.h"
#include "materialCharacter.h"
#include "materialPlayerController.h"

AmaterialGameMode::AmaterialGameMode()
{
    DefaultPawnClass = AmaterialCharacter::StaticClass();
    PlayerControllerClass = AmaterialPlayerController::StaticClass(); 

    UE_LOG(LogTemp, Log, TEXT("GameMode Constructor: PlayerControllerClass set to materialPlayerController"));
}