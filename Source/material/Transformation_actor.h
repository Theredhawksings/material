#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Transformation_actor.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPhysicalMaterial;
class ATemperature;
class AWire;

UENUM(BlueprintType)
enum class EBlockForm : uint8
{
    Ice,
    Rubber,
    Metal,
    Copper, // ★ 신규: 반자성 + 고전도율
    Wood,
    Magnet,
    None 
};

USTRUCT(BlueprintType)
struct FBlockFormSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBlockForm Form = EBlockForm::Ice;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UStaticMesh *Mesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<UMaterialInterface *> Materials;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UPhysicalMaterial *PhysMat = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSimulatePhysics = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LinearDamping = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AngularDamping = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bOverrideMass = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MassKg = 10.0f;
};

UCLASS()
class MATERIAL_API ATransformation_actor : public AActor
{
    GENERATED_BODY()

public:
    ATransformation_actor();

    void SetPowered(bool bNewPowered);
    bool IsElectrified() const { return bElectrified; }

    bool IsConductive() const { return CurrentForm == EBlockForm::Metal || CurrentForm == EBlockForm::Copper; }
    bool HasSourcePoweredWire() const;

    UPROPERTY()
    TObjectPtr<AActor> SpawnedArrowEffect;

    bool HasSourcePoweredWireRecursive(TSet<const ATransformation_actor *> &Visited) const;

    const TArray<TObjectPtr<AWire>>& GetConnectedWiresList() const { return ConnectedWires; }

    
    float GetEffectiveVoltage() const;
    float GetEffectiveCurrent() const;

    void ReceivePower(float InVoltage, float InCurrent);
    void ClearPower();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block | Pickup")
    bool bCanBePickedUp = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block | Pickup")
    bool bFixedInPlace = false;

    void SetMagneticCameraState(bool bIsCameraOn);

    UFUNCTION(BlueprintCallable, Category = "CameraSystem", meta = (WorldContext = "WorldContextObject"))
    static void SetGlobalMagneticCameraState(const UObject* WorldContextObject, bool bIsCameraOn);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnConstruction(const FTransform &Transform) override;

    bool bIsMagneticCameraOn = false;
    void RefreshArrowVisibility();  

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent *MeshComp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Form")
    EBlockForm CurrentForm = EBlockForm::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Form")
    TArray<FBlockFormSpec> FormSpecs;

    // ★ Copper 추가
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Form")
    TArray<EBlockForm> CycleOrder = {
        EBlockForm::Metal,
        EBlockForm::Copper,
        EBlockForm::Ice,
        EBlockForm::Rubber,
        EBlockForm::Wood,
        EBlockForm::Magnet};

    UFUNCTION(BlueprintCallable, Category = "Form")
    void SetForm(EBlockForm NewForm);

    UFUNCTION(BlueprintCallable, Category = "Form")
    void NextForm();

    UFUNCTION(BlueprintCallable, Category = "Ice|Heating")
    void StartHeating(ATemperature *FireRef);

    UFUNCTION()
    void ReceiveHeatEnergy(float EnergyJ, float SourceTempC);

    UFUNCTION(BlueprintCallable, Category = "Ice|Heating")
    void StopHeating();

    UFUNCTION(BlueprintCallable, Category = "Heating")
    bool IsHeating() const { return bHeating; }

    EBlockForm GetCurrentForm() const { return CurrentForm; }
    const FBlockFormSpec *FindSpec(EBlockForm Form) const;

    // ── Metal 전기 프로퍼티 (기존 그대로) ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metal|Electric")
    float WireSenseExtraRadius = 8.f;

    // ★ ── Copper 전용 프로퍼티 ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Copper|Electric")
    float CopperConductivityMultiplier = 6.0f; // Metal 대비 전도율 배수

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Copper|Electric")
    float CopperWireSenseExtraRadius = 12.f; // 더 넓은 와이어 감지

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Copper|Diamagnetic")
    float DiamagneticRepulsionForce = 800.f; // 반자성 척력 세기

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Copper|Diamagnetic")
    float DiamagneticMaxRange = 200.f; // 반자성 최대 작용 거리

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Copper|Diamagnetic")
    float DiamagneticDecayExponent = 2.0f; // 거리 감쇠 지수

    // ── Ice 프로퍼티 (기존 그대로) ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice|Visual")
    UMaterialInterface *IceMeltMaterial = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice|Visual")
    FName MeltParamName = TEXT("MeltAlpha");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice|Melt")
    float MinScaleRatio = 0.01f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice|Melt")
    bool bDestroyWhenMelted = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice|Physics")
    float IceDensityKgM3 = 917.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice|Physics")
    float LatentHeatJPerKg = 334000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice|Physics")
    float SimTimeScale = 3600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wood|Ignition")
    float WoodIgnitionTempC = 300.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wood|Combustion")
    float CombustionHeatJPerKg = 15000000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wood|Combustion")
    float BurnRateKgPerSec = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wood|Properties")
    float WoodDensityKgM3 = 600.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wood|Properties")
    float SpecificHeatJPerKgK = 1700.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wood")
    bool bDestroyWhenBurned = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wood|Visual")
    UMaterialInterface *BurnMaterial = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wood|Visual")
    FName BurnParamName = TEXT("BurnAlpha");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wood|Visual")
    float MinBurnScaleRatio = 0.05f;
    UPROPERTY(EditAnywhere, Category = "Wood|Physics")
    float WoodSimTimeScale = 100.0f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float MagnetStrength = 0.f;
    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float ReferenceDistance = 100.f;
    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float MaxLiftMass = 70.f;
    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float MinDistance = 10.f;
    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float MaxDistance = 500.f;
    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    bool bAutoComputeStrength = true;
    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float ForceMultiplier = 7.0f;
    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float MagneticDecayExponent = 1.5f;
    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float VelocityDampingFactor = 1.5f;
    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float MaxAttractVelocity = 100.f;
    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    bool bUseTorque = true;
    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    bool bApplyInitialImpulse = false;
    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float InitialImpulseStrength = 200.f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Induction")
    bool bEnableInduction = true;
    UPROPERTY(EditAnywhere, Category = "Magnet|Induction")
    float InductionStrengthRatio = 0.3f;
    UPROPERTY(EditAnywhere, Category = "Magnet|Induction")
    float InductionRange = 250.f;
    UPROPERTY(EditAnywhere, Category = "Magnet|Induction")
    float MinDistanceForInduction = 200.f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Electro")
    float WireContactRadius = 80.f;
    UPROPERTY(EditAnywhere, Category = "Magnet|Electro")
    float ElectroBoostMultiplier = 3.0f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Advanced")
    float MagnetRefreshInterval = 0.1f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Polarity")
    bool bEnablePolarity = true;
    UPROPERTY(EditAnywhere, Category = "Magnet|Polarity")
    float RepulsionMultiplier = 1.5f;
    UPROPERTY(EditAnywhere, Category = "Magnet|Polarity")
    FVector NorthPoleLocalDir = FVector(1.f, 0.f, 0.f);
    UPROPERTY(EditAnywhere, Category = "Magnet|Polarity")
    float MagnetToMagnetForceMultiplier = 15.f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Movement")
    float MagnetApproachSpeed = 300.f;
    UPROPERTY(EditAnywhere, Category = "Magnet|Movement")
    float MagnetSnapDistance = 5.f;

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bDebugDraw = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Magnet|Curie")
    bool bDemagnetized = false;

    UPROPERTY(EditAnywhere, Category = "Magnet|Curie")
    float MagnetCurieTempC = 450.f;
    UPROPERTY(EditAnywhere, Category = "Magnet|Curie")
    int32 MagnetMaxStencilValue = 220;
    UPROPERTY(EditAnywhere, Category = "Magnet|Curie", meta = (ClampMin = "0.0", ClampMax = "0.95"))
    float MagnetRecoveryRatio = 0.5f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Magnet|Electro")
    bool bElectroActive = false;

    UPROPERTY(EditAnywhere, Category = "Metal|Heat")
    int32 MetalMaxStencilValue = 220;
    UPROPERTY(EditAnywhere, Category = "Metal|Heat")
    float MetalMaxHeatTempC = 1500.f;
    UPROPERTY(EditAnywhere, Category = "Rubber|Heat")
    int32 RubberMaxStencilValue = 150;
    UPROPERTY(EditAnywhere, Category = "Rubber|Heat")
    float RubberMaxHeatTempC = 180.f;

    UPROPERTY(EditAnywhere, Category = "Form|Heat")
    float FormCoolingRatePerSec = 30.f;
    UPROPERTY(EditAnywhere, Category = "Form|Heat")
    float FormHeatSimTimeScale = 500.f;
    UPROPERTY(EditAnywhere, Category = "Form|Heat")
    float FormMassKg = 5.f;
    UPROPERTY(EditAnywhere, Category = "Form|Heat")
    float FormSpecificHeatJPerKgK = 500.f;

    // ── 태그 ──
    UPROPERTY(EditAnywhere, Category = "Transformation|Tags")
    bool bAutoUpdateTags = true;
    UPROPERTY(EditAnywhere, Category = "Transformation|Tags")
    FName IceTag = "Ice";
    UPROPERTY(EditAnywhere, Category = "Transformation|Tags")
    FName MetalTag = "Metal";
    UPROPERTY(EditAnywhere, Category = "Transformation|Tags")
    FName CopperTag = "Copper"; // ★ 신규
    UPROPERTY(EditAnywhere, Category = "Transformation|Tags")
    FName WoodTag = "Wood";
    UPROPERTY(EditAnywhere, Category = "Transformation|Tags")
    FName RubberTag = "Rubber";
    UPROPERTY(EditAnywhere, Category = "Transformation|Tags")
    FName MagnetTag = "Magnet";

    FVector GetNorthPoleWorldDir() const;
    FVector GetSouthPoleWorldDir() const;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Electrical")
float MetalResistance = 0.6f;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Electrical")
float CopperResistance = 0.1f;

UPROPERTY(VisibleAnywhere, Category = "Electrical")
float BlockResistance = 0.f;

private:
    // ── 전기 (Metal + Copper 공용) ──
    void RefreshConnectedWires();
    void SetElectrified(bool bNewElectrified);
    void EnergizeWiresIfElectrified();

    // ── 태그/스펙 ──
    void UpdateTagsForForm(EBlockForm Form);
    void ClearAllFormTags();
    void ApplySpec(const FBlockFormSpec &Spec);

    // ── 얼음 ──
    void EnterIceMode();
    void ExitIceMode();
    void RecalcIceMassAndEnergy();
    void ApplyIceMeltVisual(float Alpha01);

    // ── 나무 ──
    void EnterWoodMode();
    void ExitWoodMode();
    void RecalcWoodMassAndVolume();
    void ApplyWoodBurnVisual(float Alpha01);

    // ── 자석 ──
    void EnterMagnetMode();
    void ExitMagnetMode();
    void UpdateMagnetism(float DeltaTime);
    void RefreshOverlappingMetals();
    void DecreaseGaugeForCurrentTag();

    // ── 열/소자 ──
    void UpdateFormHeat(float DeltaTime);
    void UpdateMagnetArrowPower(float PowerRatio);
    void UpdateMagnetElectroBoost();
    void ApplyInducedMagnetism();
    float CalculateInducedStrength(float DistanceToMagnet, float BaseMagnetStrengthVal) const;

    // ── 공용 유틸리티 ──
    float CalcReceivedPower(float DistCm) const;
    void SetStencilSafe(int32 NewValue, bool bDepthOn);

    // ★ 전도체 와이어 감지 반경 (Metal/Copper에 따라 다름)
    float GetWireSenseRadius() const;

private:
    UPROPERTY(Transient)
    UMaterialInstanceDynamic *IceMID = nullptr;
    UPROPERTY(Transient)
    ATemperature *CurrentFire = nullptr;

    bool bHeating = false;
    float MeltAlpha = 0.0f;
    float EnergyAccumJ = 0.0f;
    float VolumeM3 = 1.0f;
    float EffectiveAreaM2 = 1.0f;
    float TotalMeltEnergyJ = 1.0f;
    FVector BaseScaleBeforeMelt = FVector(1.0f);

    UPROPERTY(Transient)
    UMaterialInstanceDynamic *BurnMID = nullptr;

    float WoodTemperatureC = 20.0f;
    float WoodVolumeM3 = 1.0f;
    float WoodMassKg = 1.0f;
    float CurrentWoodMassKg = 1.0f;
    float BurnAlpha = 0.0f;
    bool bIsBurning = false;
    FVector BaseScaleBeforeBurn = FVector(1.0f);

    UPROPERTY(Transient)
    TArray<TObjectPtr<AWire>> ConnectedWires;
    bool bElectrified = false;
    TSet<TWeakObjectPtr<AWire>> WiresEnergizedByMetal;
    FTimerHandle RefreshTimerHandle;

    UPROPERTY(Transient)
    TSet<TObjectPtr<UPrimitiveComponent>> OverlappingMetals;
    UPROPERTY(Transient)
    TSet<TObjectPtr<UPrimitiveComponent>> OverlappingCoppers;
    UPROPERTY(Transient)
    TArray<TObjectPtr<AWire>> MagnetContactedWires;

    float TimeSinceLastMagnetRefresh = 0.f;
    float BaseMagnetStrength = 0.f;
    float FormTemperatureC = 20.f;
    float BaseArrowPower = 5.f;

    int32 CachedStencilValue = -1;
    bool bCachedDepthOn = false;

    UPROPERTY(Transient)
    TSet<TObjectPtr<UPrimitiveComponent>> PreviousOverlappingMetals;

    UFUNCTION()
    void OnMagnetHit(UPrimitiveComponent *HitComp, AActor *OtherActor,
                     UPrimitiveComponent *OtherComp, FVector NormalImpulse, const FHitResult &Hit);

    bool bMagnetCollided = false;
    bool bMagnetSnapped = false;

    static constexpr float MaxForceClamp = 5e5f;
    static constexpr float MaxInducedForceClamp = 3e7f;
    static constexpr float GravityAccel = 980.f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Visual")
    TSubclassOf<AActor> ArrowEffectClass;
    UPROPERTY(EditAnywhere, Category = "Magnet|Visual")
    float ArrowPower = 5.0f;
    UPROPERTY(EditAnywhere, Category = "Magnet|Visual")
    float ArrowX = 100.0f;
    UPROPERTY(EditAnywhere, Category = "Magnet|Visual")
    float ArrowY = 100.0f;
    UPROPERTY(EditAnywhere, Category = "Magnet|Visual")
    bool bShowFieldArrows = true;

    float StoredVoltage = 0.f;
    float StoredCurrent = 0.f;

private:
    void ApplyFixedPhysics();

    UPROPERTY(EditAnywhere, Category = "Rubber|Bounce")
    float RubberBounceMultiplier = 2.5f;

    UFUNCTION()
    void OnRubberHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);


    UPROPERTY(EditAnywhere, Category = "Rubber|Bounce")
    float RubberBounceDecay = 0.8f;

    float CurrentBounceMultiplier = RubberBounceMultiplier; 
    
    UPROPERTY(EditAnywhere, Category = "Ice|Visual")
    TObjectPtr<UNiagaraSystem> SteamEffect = nullptr;
 
    UPROPERTY(Transient)
    TObjectPtr<UNiagaraComponent> SteamComponent = nullptr;
 
    void UpdateSteamEffect();
    void SpawnSteamEffect();
    void DestroySteamEffect();
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

   bool bPendingDelayedDestroy = false;
    void BeginDelayedDestroy();
    FTimerHandle ResidualStopHandle;
    FTimerHandle ResidualKillHandle;
};