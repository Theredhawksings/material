#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Battery.h"
#include "BatteryOffTrigger.generated.h"

class AGenerator;
class AIronSpawner;

UCLASS()
class MATERIAL_API ABatteryOffTrigger : public AActor
{
    GENERATED_BODY()

public:
    ABatteryOffTrigger();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battery")
    TObjectPtr<ABATTERY> TargetBattery;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Targets")
    TObjectPtr<AGenerator> TargetGenerator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Targets")
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