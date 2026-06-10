#pragma once

#include "CoreMinimal.h"
#include "Temperature.h"
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

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> WireframeMeshComp;

    void HeatNearbyTemperatureBlocks(float DeltaTime);
};