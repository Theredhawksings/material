#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "AirConditioner.h"
#include "TransformPowerTrigger.generated.h"

UCLASS()
class MATERIAL_API ATransformPowerTrigger : public AActor
{
    GENERATED_BODY()

public:
    ATransformPowerTrigger();

    // ── 에어컨 참조 (에디터에서 지정) ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Targets")
    TObjectPtr<AAirConditioner> TargetAircon;

    // ── KeyTag 그대로 유지 ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Targets")
    FName KeyTag = TEXT("PowerKey");

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> TriggerBox;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);
};