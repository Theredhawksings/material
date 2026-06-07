#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "MoveStage1_2.generated.h"

UCLASS()
class MATERIAL_API AMoveStage1_2 : public AActor
{
    GENERATED_BODY()

public:
    AMoveStage1_2();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* TriggerBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComponent;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    void LoadNextLevel();

    FTimerHandle LevelLoadTimerHandle;

public:
    virtual void Tick(float DeltaTime) override;

    // 이동할 레벨 이름 (예: MainStage1)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
    FName LevelToLoad = FName("MainStage1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
    float LoadDelay = 3.0f;

    // 에디터에서 원하는 스폰 위치/회전 직접 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
    FVector SpawnLocation = FVector(0.f, 0.f, 300.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
    FRotator SpawnRotation = FRotator::ZeroRotator;
};