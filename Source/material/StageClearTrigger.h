#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "StageClearTrigger.generated.h"

class AGenerator;
class AIronSpawner;
class AAirConditioner;   // ★ 추가

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

    // ★ 스테이지 클리어 시 멈출 에어컨
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Clear")
    TObjectPtr<AAirConditioner> TargetAircon;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> TriggerBox;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);
};