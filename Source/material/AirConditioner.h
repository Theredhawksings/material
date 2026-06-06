#pragma once

#include "CoreMinimal.h"
#include "Temperature.h"
#include "Components/BoxComponent.h"
#include "AirConditioner.generated.h"

UCLASS()
class MATERIAL_API AAirConditioner : public ATemperature
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
    // true  → 게임 시작하자마자 무조건 ON (스위치 불필요)
    // false → PowerTrigger 가 ActivateAircon() 불러줘야 ON
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner")
    bool bAlwaysOn = false;

    // ── 에어컨 작동 시 올라가는 가열 온도 ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner")
    float HeatTemperature = 600.0f;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ── 얼음 감지 박스 (디버깅 용도) ──
    UFUNCTION()
    void OnDetectionOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnDetectionOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    UPROPERTY(EditAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> DetectionBox;
};