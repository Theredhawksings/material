#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "StageClearTrigger.generated.h"

class AGenerator;
class AIronSpawner;

UCLASS()
class MATERIAL_API AStageClearTrigger : public AActor
{
    GENERATED_BODY()

public:
    AStageClearTrigger();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Clear")
    TObjectPtr<AGenerator> TargetGenerator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Clear")
    TObjectPtr<AIronSpawner> TargetSpawner;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> TriggerBox;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);
};