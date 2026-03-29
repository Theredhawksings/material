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
    USplineComponent* GetSplineComponent() const { return Spline; }

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
    FVector2D SegmentScale = FVector2D(0.15f, 0.15f);

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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wire|Electrical")
    float BatteryVoltage = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Electrical")
    float Resistance = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Electrical")
    float DefaultVoltage = 12.0f;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|HeatEmit")
    float HeatEmitRadius = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|HeatEmit")
    float HeatEmitThresholdC = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|HeatEmit")
    float WireEmissivity = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|HeatEmit")
    float WireSurfaceAreaM2 = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visual")
    FName WireHeatParamName = TEXT("HeatAlpha");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visual")
    float WireTempVisualScale = 0.002f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|IceHeat")
    float IceHeatZoneRadius = 350.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|IceHeat")
    float IceHeatThresholdC = 80.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|IceHeat")
    float IceHeatMultiplier = 10000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|IceHeat")
    float IceReceiveAreaM2 = 0.20f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|IceHeat")
    float SegmentHeatMultiplier = 50.f;

private:
    void ClearGeneratedMeshes();
    void UpdateConnectionPoint();
    void UpdateFinalPower();
    void PropagatePowerToConnected();
    void UpdateJouleHeating(float DeltaTime);
    void EmitHeatToNearby(float DeltaTime);
    void UpdateWireVisual();

    UFUNCTION()
    void OnIceHeatZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnIceHeatZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    UPROPERTY()
    TArray<TObjectPtr<USplineMeshComponent>> SegmentMeshes;

    UPROPERTY()
    TArray<TObjectPtr<USphereComponent>> HeatSpheres;

    UPROPERTY()
    TObjectPtr<USphereComponent> IceHeatZone;

    UPROPERTY()
    TArray<TObjectPtr<AActor>> ConnectedActors;

    UPROPERTY()
    TArray<TObjectPtr<UMaterialInstanceDynamic>> SegmentMIDs;

    FTimerHandle RefreshTimerHandle;

    bool bPoweredBySource = false;
    bool bPoweredByMetal = false;

    float CurrentAmps = 0.f;

    static constexpr float StefanBoltzmannSigma = 5.67e-8f;
};