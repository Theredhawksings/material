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
class UNiagaraSystem;      
class UNiagaraComponent;
class ATransformation_actor;

UENUM(BlueprintType)
enum class EWireMaterial : uint8
{
    Neutral,
    Copper,   // 구리: 1Ω
    Iron,     // 철:  3Ω
};

UCLASS()
class MATERIAL_API AWire : public AActor
{
    GENERATED_BODY()

public:
    AWire();

    void SetPowered(bool bNewPowered, bool bStartIsInput = true);
    FVector GetStartPointLocation() const;
    FVector GetEndPointLocation() const;

    void SetPoweredByMetal(bool bNewPoweredByMetal);
    void SetBatteryVoltage(float NewVoltage);

    bool  IsPowered()       const { return bPoweredFinal; }
    bool  IsSourcePowered() const { return bPoweredBySource; }
    float GetWireTemperature()  const { return WireTemperatureC; }
    float GetEffectiveVoltage() const { return EffectiveVoltage; }
    float GetEffectiveCurrent() const { return EffectiveCurrent; }
    USplineComponent* GetSplineComponent() const { return Spline; }

    void RefreshConnectedActors();
    void ApplyPower();
    void RebuildSplineMeshes();

    void BuildCircuitGraph(TMap<AWire*, int32>& IncomingCountMap, TSet<AWire*>& Visited);
    void PropagateVoltage(float IncomingVoltage, float IncomingCurrent,
                          TMap<AWire*, float>& VoltageMap,
                          TMap<AWire*, float>& CurrentAccumMap,
                          const TMap<AWire*, int32>& IncomingCountMap);
    void ResetVoltageNetwork(TSet<AWire*>& Visited);

    const TArray<TObjectPtr<AActor>>& GetConnectedActors() const { return ConnectedActors; }
    const TArray<TObjectPtr<AWire>>&  GetConnectedWires()  const { return ConnectedWires; }

    void SetBatterySource(bool bIsSource) { bIsBatterySource = bIsSource; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Source")
    TObjectPtr<AWire> SourceWire = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Source")
    TObjectPtr<ATransformation_actor> SourceBlock = nullptr;
    
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    UPROPERTY(VisibleAnywhere, Category = "Wire|Components")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, Category = "Wire|Components")
    TObjectPtr<USplineComponent> Spline;

    UPROPERTY(VisibleAnywhere, Category = "Wire|Components")
    TObjectPtr<USphereComponent> ConnectionSphere;

    UPROPERTY(VisibleAnywhere, Category = "Wire|Components")
    TObjectPtr<USphereComponent> ConnectionSphereEnd;

    UPROPERTY(EditAnywhere, Category = "Wire|Build")
    TObjectPtr<UStaticMesh> SegmentMesh;

    UPROPERTY(EditAnywhere, Category = "Wire|Build")
    FVector2D SegmentScale = FVector2D(0.03f, 0.03f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Build")
    EWireMaterial WireMaterial = EWireMaterial::Neutral;

    UPROPERTY(EditAnywhere, Category = "Wire|Visual")
    TObjectPtr<UMaterialInterface> OffMaterial;

    UPROPERTY(EditAnywhere, Category = "Wire|Visual")
    TObjectPtr<UMaterialInterface> OnMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visual")
    FName WireHeatParamName = TEXT("HeatAlpha");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visual")
    float WireTempVisualScale = 0.002f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wire|Power")
    bool bPoweredFinal = false;

    UPROPERTY(EditAnywhere, Category = "Wire|Connection")
    float OverlapRadius = 30.f;

    UPROPERTY(EditAnywhere, Category = "Wire|Connection")
    float RefreshInterval = 0.10f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Debug")
    bool bDebugWire = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Debug")
    bool bDebugCircuit = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Debug")
    bool bShowDebugShapes = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wire|Electrical")
    float BatteryVoltage = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Electrical")
    float Resistance = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Electrical")
    float DefaultVoltage = 12.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wire|Electrical")
    float EffectiveVoltage = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wire|Electrical")
    float EffectiveCurrent = 0.f;

    // ★ 꼼수: 이 전선이 병렬 가지인지 체크
    // true면 ParallelBranchCount로 전류를 나눔
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Electrical")
    bool bIsParallel = false;

    // ★ 병렬 가지 수 (bIsParallel=true일 때만 사용)
    // 예) 2개 병렬이면 2, 3개 병렬이면 3
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Electrical",
        meta = (ClampMin = "1", EditCondition = "bIsParallel"))
    int32 ParallelBranchCount = 2;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|HeatEmit")
    float HeatEmitInterval = 0.1f;

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
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Thermal")
    float MergeFollowRate = 3.f;   // 합류 전선이 기준 상류 온도 따라붙는 속도(1/초)

private:
    void ClearGeneratedMeshes();
    void UpdateConnectionPoint();
    void UpdateFinalPower();
    void PropagatePowerToConnected();
    void UpdateJouleHeating(float DeltaTime);
    void EmitHeatToNearby(float DeltaTime);
    void UpdateWireVisual();
    void ApplyDebugVisibility();
    void TriggerCircuitSolve();

    float CalcSeriesResistance(TSet<AWire*>& Visited) const;
    void CollectNextWires(TArray<AWire*>& Out, const TMap<AWire*, float>& VoltageMap) const;

    UFUNCTION()
    void OnIceHeatZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnIceHeatZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    UPROPERTY()
    TArray<TObjectPtr<USplineMeshComponent>> SegmentMeshes;

    UPROPERTY()
    TArray<TObjectPtr<USphereComponent>> HeatSpheres;

    UPROPERTY()
    TObjectPtr<USphereComponent> IceHeatZone;

    UPROPERTY()
    TArray<TObjectPtr<AActor>> ConnectedActors;

    UPROPERTY()
    TArray<TObjectPtr<AWire>> ConnectedWires;

    UPROPERTY()
    TArray<TObjectPtr<UMaterialInstanceDynamic>> SegmentMIDs;

    FTimerHandle RefreshTimerHandle;

    bool  bPoweredBySource       = false;
    bool  bPoweredByMetal        = false;
    bool  bLastAppliedPowerState = false;
    bool  bInputIsStart          = true;

    float CurrentAmps            = 0.f;
    float HeatEmitAccumulator    = 0.f;
    int32 CachedWireStencilValue = -1;
    float CachedWireHeatAlpha    = -1.f;

    FString CachedCircuitText;
    FColor  CachedCircuitColor = FColor::White;

    TSet<ATransformation_actor*> CachedHeatTargets;

    static constexpr float StefanBoltzmannSigma = 5.67e-8f;


    bool bCircuitSolved = false;
    bool bIsBatterySource = false;

    UNiagaraSystem* SparkEffect = nullptr;
    UNiagaraComponent* SparkComponentStart = nullptr;
    UNiagaraComponent* SparkComponentEnd = nullptr;


    bool  bIsMergeNode      = false;
    float HeatFollowTargetC = -1.f;
};
