#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Battery.h"
#include "Transformation_actor.h" // 전방 선언 대신 바로 포함시켜 형태(Form) 확인 등에 대비
#include "TransformPowerTrigger.generated.h"

UCLASS()
class MATERIAL_API ATransformPowerTrigger : public AActor
{
    GENERATED_BODY()

public:
    ATransformPowerTrigger();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Targets")
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