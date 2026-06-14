#pragma once

#include "CoreMinimal.h"
#include "Temperature.h"
#include "AirConditioner.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner")
    bool bAlwaysOn = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner")
    float HeatTemperature = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner|Physics")
    float BlockMassKg = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner|Physics")
    float BlockSpecificHeatJPerKgK = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner|Physics")
    float BlockReceiverAreaM2 = 0.01f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner|Physics")
    float HeatSimTimeScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner|Debug")
    bool bDebugHeat = false;

    // ★ 연기(나이아가라) 관련
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner|Smoke")
    TObjectPtr<UNiagaraSystem> SmokeEffect;

    // 에디터에서 켜고 끌 수 있는 연기 분사율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner|Smoke")
    float SmokeSpawnRate = 50.f;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ★ 연기 노즐 9개 (위치는 에디터에서 조정)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AirConditioner|Smoke")
    TArray<TObjectPtr<UNiagaraComponent>> SmokeComponents;

    void SetSmokeActive(bool bActive);

private:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> WireframeMeshComp;

    void HeatNearbyTemperatureBlocks(float DeltaTime);

    static constexpr int32 NumSmokeNozzles = 9;
};