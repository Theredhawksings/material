#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "materialGameMode.generated.h"

UCLASS()
class MATERIAL_API AmaterialGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AmaterialGameMode();

    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;

private:
    FVector CustomSpawnLocation;
    FRotator CustomSpawnRotation;
    bool bUseCustomSpawn = false;
};