#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Wire.generated.h"

class USceneComponent;
class USplineComponent;
class USplineMeshComponent;
class UMaterialInterface;
class USphereComponent;
class UStaticMesh;
class ATransformation_actor;

UCLASS()
class MATERIAL_API AWire : public AActor
{
    GENERATED_BODY()

public:
    AWire();

    void SetPowered(bool bNewPowered);
    void SetPoweredByMetal(bool bNewPoweredByMetal);
    void SetBatteryVoltage(float NewVoltage);

    bool IsPowered() const { return bPoweredFinal; }
    bool IsSourcePowered() const { return bPoweredBySource; }
    float GetWireTemperature() const { return WireTemperatureC; }

    void RefreshConnectedActors();
    void ApplyPower();
    void RebuildSplineMeshes();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    UPROPERTY(VisibleAnywhere, Category = "Wire|Components")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, Category = "Wire|Components")
    TObjectPtr<USplineComponent> Spline;

    UPROPERTY(VisibleAnywhere, Category = "Wire|Components")
    TObjectPtr<USphereComponent> ConnectionSphere;

    UPROPERTY(EditAnywhere, Category = "Wire|Build")
    TObjectPtr<UStaticMesh> SegmentMesh;

    UPROPERTY(EditAnywhere, Category = "Wire|Build")
    FVector2D SegmentScale = FVector2D(0.03f, 0.03f);

    UPROPERTY(EditAnywhere, Category = "Wire|Visual")
    TObjectPtr<UMaterialInterface> OffMaterial;

    UPROPERTY(EditAnywhere, Category = "Wire|Visual")
    TObjectPtr<UMaterialInterface> OnMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wire|Power")
    bool bPoweredFinal = false;

    UPROPERTY(EditAnywhere, Category = "Wire|Connection")
    float OverlapRadius = 30.f;

    UPROPERTY(EditAnywhere, Category = "Wire|Connection")
    float RefreshInterval = 0.10f;

    UPROPERTY(EditAnywhere, Category = "Wire|Debug")
    bool bDebugWire = true;

    // ── 전기 ──
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wire|Electrical")
    float BatteryVoltage = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Electrical")
    float Resistance = 2.0f;

    // ── 줄 발열 (Joule Heating) ──
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wire|Thermal")
    float WireTemperatureC = 20.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Thermal")
    float MaxWireTemperatureC = 800.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Thermal")
    float AmbientTemperatureC = 20.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Thermal")
    float WireMassKg = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Thermal")
    float SpecificHeatJPerKgK = 385.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Thermal")
    float CoolingRateKPerSec = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Thermal")
    float SimTimeScale = 50.f;

    // ── 열 방출 ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|HeatEmit")
    float HeatEmitRadius = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|HeatEmit")
    float HeatEmitThresholdC = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|HeatEmit")
    float WireEmissivity = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|HeatEmit")
    float WireSurfaceAreaM2 = 0.1f;

private:
    void ClearGeneratedMeshes();
    void UpdateConnectionPoint();
    void UpdateFinalPower();
    void PropagatePowerToConnected();
    void UpdateJouleHeating(float DeltaTime);
    void EmitHeatToNearby(float DeltaTime);

private:
    UPROPERTY()
    TArray<TObjectPtr<USplineMeshComponent>> SegmentMeshes;

    UPROPERTY()
    TArray<TObjectPtr<AActor>> ConnectedActors;

    FTimerHandle RefreshTimerHandle;

    bool bPoweredBySource = false;
    bool bPoweredByMetal = false;

    float CurrentAmps = 0.f;

    static constexpr float StefanBoltzmannSigma = 5.67e-8f;
};