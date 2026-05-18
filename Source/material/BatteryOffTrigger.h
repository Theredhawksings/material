#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Battery.h"
#include "BatteryOffTrigger.generated.h"

UCLASS()
class MATERIAL_API ABatteryOffTrigger : public AActor
{
    GENERATED_BODY()

public:
    ABatteryOffTrigger();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battery")
    TObjectPtr<ABATTERY> TargetBattery;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> TriggerBox;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);
};