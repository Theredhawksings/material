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

    bool IsPowered()       const { return bPoweredFinal; }
    bool IsSourcePowered() const { return bPoweredBySource; }
    float GetWireTemperature()  const { return WireTemperatureC; }
    float GetEffectiveVoltage() const { return EffectiveVoltage; }
    float GetEffectiveCurrent() const { return EffectiveCurrent; }
    USplineComponent* GetSplineComponent() const { return Spline; }

    void RefreshConnectedActors();
    void ApplyPower();
    void RebuildSplineMeshes();

    // ── 회로 해석 (2패스) ──────────────────────────────────
    // 패스 1: 그래프 탐색 → 각 전선에 몇 개 경로가 들어오는지 카운트
    void BuildCircuitGraph(TMap<AWire*, int32>& IncomingCountMap,
                           TSet<AWire*>&        Visited);

    // 패스 2: 전압/전류 전파
    //   VoltageMap     : 이미 전압이 설정된 전선 (병렬 합산 지점 감지)
    //   CurrentAccumMap: 병렬 합산 지점에서 전류 누적
    //   IncomingCountMap: 각 전선에 들어오는 경로 수
    void PropagateVoltage(float IncomingVoltage,
                          float IncomingCurrent,
                          TMap<AWire*, float>&       VoltageMap,
                          TMap<AWire*, float>&       CurrentAccumMap,
                          const TMap<AWire*, int32>& IncomingCountMap);

    // 전원 OFF 시 연결망 전체 초기화
    void ResetVoltageNetwork(TSet<AWire*>& Visited);

    const TArray<TObjectPtr<AActor>>& GetConnectedActors() const { return ConnectedActors; }
    const TArray<TObjectPtr<AWire>>&  GetConnectedWires()  const { return ConnectedWires; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    // ── Components ──────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, Category = "Wire|Components")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, Category = "Wire|Components")
    TObjectPtr<USplineComponent> Spline;

    UPROPERTY(VisibleAnywhere, Category = "Wire|Components")
    TObjectPtr<USphereComponent> ConnectionSphere;      // 시작점

    UPROPERTY(VisibleAnywhere, Category = "Wire|Components")
    TObjectPtr<USphereComponent> ConnectionSphereEnd;   // 끝점

    // ── Build ────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, Category = "Wire|Build")
    TObjectPtr<UStaticMesh> SegmentMesh;

    UPROPERTY(EditAnywhere, Category = "Wire|Build")
    FVector2D SegmentScale = FVector2D(0.03f, 0.03f);

    // ── Visual ───────────────────────────────────────────────
    UPROPERTY(EditAnywhere, Category = "Wire|Visual")
    TObjectPtr<UMaterialInterface> OffMaterial;

    UPROPERTY(EditAnywhere, Category = "Wire|Visual")
    TObjectPtr<UMaterialInterface> OnMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visual")
    FName WireHeatParamName = TEXT("HeatAlpha");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Visual")
    float WireTempVisualScale = 0.002f;

    // ── Power ────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wire|Power")
    bool bPoweredFinal = false;

    // ── Connection ───────────────────────────────────────────
    UPROPERTY(EditAnywhere, Category = "Wire|Connection")
    float OverlapRadius = 30.f;

    UPROPERTY(EditAnywhere, Category = "Wire|Connection")
    float RefreshInterval = 0.10f;

    // ── Debug ────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Debug")
    bool bDebugWire = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Debug")
    bool bShowDebugShapes = true;

    // true 시 화면에 직렬/병렬/다이아몬드 타입과 전압·전류 출력
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Debug")
    bool bDebugCircuit = false;

    // ── Electrical ───────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wire|Electrical")
    float BatteryVoltage = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Electrical")
    float Resistance = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Electrical")
    float DefaultVoltage = 12.0f;

    // 회로 계산 결과 (에디터 디테일 패널에서 실시간 확인 가능)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wire|Electrical")
    float EffectiveVoltage = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wire|Electrical")
    float EffectiveCurrent = 0.f;

    // ── Thermal ──────────────────────────────────────────────
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

    // ── HeatEmit ─────────────────────────────────────────────
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

    // ── IceHeat ──────────────────────────────────────────────
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
    void ApplyDebugVisibility();
    void TriggerCircuitSolve();   // 소스 탐색 후 2패스 회로 해석 실행

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

    bool  bPoweredBySource      = false;
    bool  bPoweredByMetal       = false;
    bool  bLastAppliedPowerState = false;

    float CurrentAmps           = 0.f;
    float HeatEmitAccumulator   = 0.f;
    int32 CachedWireStencilValue = -1;
    float CachedWireHeatAlpha    = -1.f;

    TSet<ATransformation_actor*> CachedHeatTargets;

    static constexpr float StefanBoltzmannSigma = 5.67e-8f;
};