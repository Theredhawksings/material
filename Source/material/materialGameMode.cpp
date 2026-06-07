#include "materialGameMode.h"
#include "materialCharacter.h"
#include "materialPlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

AmaterialGameMode::AmaterialGameMode()
{
    DefaultPawnClass = AmaterialCharacter::StaticClass();
    PlayerControllerClass = AmaterialPlayerController::StaticClass();

    UE_LOG(LogTemp, Warning, TEXT("GameMode Constructor - PlayerControllerClass: %s"), 
           PlayerControllerClass ? *PlayerControllerClass->GetName() : TEXT("NULL"));
}

void AmaterialGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    FString SpawnXStr = UGameplayStatics::ParseOption(Options, TEXT("SpawnX"));
    FString SpawnYStr = UGameplayStatics::ParseOption(Options, TEXT("SpawnY"));
    FString SpawnZStr = UGameplayStatics::ParseOption(Options, TEXT("SpawnZ"));

    if (!SpawnXStr.IsEmpty() && !SpawnYStr.IsEmpty() && !SpawnZStr.IsEmpty())
    {
        CustomSpawnLocation = FVector(
            FCString::Atof(*SpawnXStr),
            FCString::Atof(*SpawnYStr),
            FCString::Atof(*SpawnZStr)
        );
        CustomSpawnRotation = FRotator(
            FCString::Atof(*UGameplayStatics::ParseOption(Options, TEXT("SpawnPitch"))),
            FCString::Atof(*UGameplayStatics::ParseOption(Options, TEXT("SpawnYaw"))),
            FCString::Atof(*UGameplayStatics::ParseOption(Options, TEXT("SpawnRoll")))
        );
        bUseCustomSpawn = true;
    }
}

AActor* AmaterialGameMode::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
    if (bUseCustomSpawn)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        APlayerStart* TempStart = GetWorld()->SpawnActor<APlayerStart>(
            APlayerStart::StaticClass(), CustomSpawnLocation, CustomSpawnRotation, Params);

        if (TempStart)
        {
            bUseCustomSpawn = false; // 한 번만 사용
            return TempStart;
        }
    }

    return Super::FindPlayerStart_Implementation(Player, IncomingName);
}