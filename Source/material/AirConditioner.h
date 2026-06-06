#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AirConditioner.generated.h"

UCLASS()
class MATERIAL_API AAirConditioner : public AActor
{
    GENERATED_BODY()

public:
    AAirConditioner();

    UFUNCTION(BlueprintCallable, Category = "AirConditioner")
    void ActivateAircon();

    UFUNCTION(BlueprintCallable, Category = "AirConditioner")
    void DeactivateAircon();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AirConditioner")
    bool bIsRunning = false;

    // ── 에디터에서 상시 활성화 여부 설정 ──
    // true  → 게임 시작하자마자 무조건 ON (트리거 필요 없음)
    // false → TransformPowerTrigger 또는 DetectionBox 로만 ON 가능
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner")
    bool bAlwaysOn = false;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ── 감지 박스 Overlap 이벤트 ──
    UFUNCTION()
    void OnDetectionOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnDetectionOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComp;

    UPROPERTY(EditAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> DetectionBox;
};