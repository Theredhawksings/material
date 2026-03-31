#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
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
    Wood,
    Magnet
};

USTRUCT(BlueprintType)
struct FBlockFormSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBlockForm Form = EBlockForm::Ice;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UStaticMesh* Mesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<UMaterialInterface*> Materials;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UPhysicalMaterial* PhysMat = nullptr;

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
    
    UPROPERTY()
    TObjectPtr<AActor> SpawnedArrowEffect;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnConstruction(const FTransform& Transform) override;

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    UStaticMeshComponent* MeshComp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Form")
    EBlockForm CurrentForm = EBlockForm::Ice;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Form")
    TArray<FBlockFormSpec> FormSpecs;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Form")
    TArray<EBlockForm> CycleOrder = { EBlockForm::Metal, EBlockForm::Ice, EBlockForm::Rubber, EBlockForm::Wood, EBlockForm::Magnet };

    UFUNCTION(BlueprintCallable, Category="Form")
    void SetForm(EBlockForm NewForm);

    UFUNCTION(BlueprintCallable, Category="Form")
    void NextForm();

    UFUNCTION(BlueprintCallable, Category="Ice|Heating")
    void StartHeating(ATemperature* FireRef);

    UFUNCTION()
    void ReceiveHeatEnergy(float EnergyJ, float SourceTempC);

    UFUNCTION(BlueprintCallable, Category="Ice|Heating")
    void StopHeating();

    UFUNCTION(BlueprintCallable, Category="Heating")
    bool IsHeating() const { return bHeating; }

    EBlockForm GetCurrentForm() const { return CurrentForm; }
    const FBlockFormSpec* FindSpec(EBlockForm Form) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Metal|Electric")
    float WireSenseExtraRadius = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Visual")
    UMaterialInterface* IceMeltMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Visual")
    FName MeltParamName = TEXT("MeltAlpha");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Melt")
    float MinScaleRatio = 0.01f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Melt")
    bool bDestroyWhenMelted = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Physics")
    float IceDensityKgM3 = 917.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Physics")
    float LatentHeatJPerKg = 334000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Physics")
    float SimTimeScale = 3600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wood|Ignition")
    float WoodIgnitionTempC = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wood|Combustion")
    float CombustionHeatJPerKg = 15000000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wood|Combustion")
    float BurnRateKgPerSec = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wood|Properties")
    float WoodDensityKgM3 = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wood|Properties")
    float SpecificHeatJPerKgK = 1700.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wood")
    bool bDestroyWhenBurned = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wood|Visual")
    UMaterialInterface* BurnMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wood|Visual")
    FName BurnParamName = TEXT("BurnAlpha");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wood|Visual")
    float MinBurnScaleRatio = 0.05f;

    UPROPERTY(EditAnywhere, Category="Wood|Physics")
    float WoodSimTimeScale = 100.0f;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float MagnetStrength = 0.f;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float ReferenceDistance = 100.f;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float MaxLiftMass = 70.f;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float MinDistance = 10.f;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float MaxDistance = 500.f;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    bool bAutoComputeStrength = true;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float ForceMultiplier = 7.0f;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float MagneticDecayExponent = 1.5f;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float VelocityDampingFactor = 1.5f;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float MaxAttractVelocity = 100.f;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    bool bUseTorque = true;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    bool bApplyInitialImpulse = false;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float InitialImpulseStrength = 200.f;

    UPROPERTY(EditAnywhere, Category="Magnet|Induction")
    bool bEnableInduction = true;

    UPROPERTY(EditAnywhere, Category="Magnet|Induction")
    float InductionStrengthRatio = 0.3f;

    UPROPERTY(EditAnywhere, Category="Magnet|Induction")
    float InductionRange = 250.f;

    UPROPERTY(EditAnywhere, Category="Magnet|Induction")
    float MinDistanceForInduction = 200.f;

    UPROPERTY(EditAnywhere, Category="Magnet|Electro")
    float WireContactRadius = 80.f;

    UPROPERTY(EditAnywhere, Category="Magnet|Electro")
    float ElectroBoostMultiplier = 3.0f;

    UPROPERTY(EditAnywhere, Category="Magnet|Advanced")
    float MagnetRefreshInterval = 0.1f;

    UPROPERTY(EditAnywhere, Category="Magnet|Polarity")
    bool bEnablePolarity = true;

    UPROPERTY(EditAnywhere, Category="Magnet|Polarity")
    float RepulsionMultiplier = 1.5f;

    UPROPERTY(EditAnywhere, Category="Magnet|Polarity")
    FVector NorthPoleLocalDir = FVector(1.f, 0.f, 0.f);

    UPROPERTY(EditAnywhere, Category="Magnet|Polarity")
    float MagnetToMagnetForceMultiplier = 15.f;

    UPROPERTY(EditAnywhere, Category="Magnet|Movement")
    float MagnetApproachSpeed = 300.f;

    UPROPERTY(EditAnywhere, Category="Magnet|Movement")
    float MagnetSnapDistance = 5.f;

    UPROPERTY(EditAnywhere, Category="Debug")
    bool bDebugDraw = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Magnet|Curie")
    bool bDemagnetized = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Magnet|Electro")
    bool bElectroActive = false;

    UPROPERTY(EditAnywhere, Category="Transformation|Tags")
    bool bAutoUpdateTags = true;

    UPROPERTY(EditAnywhere, Category="Transformation|Tags")
    FName IceTag = "Ice";

    UPROPERTY(EditAnywhere, Category="Transformation|Tags")
    FName MetalTag = "Metal";

    UPROPERTY(EditAnywhere, Category="Transformation|Tags")
    FName WoodTag = "Wood";

    UPROPERTY(EditAnywhere, Category="Transformation|Tags")
    FName RubberTag = "Rubber";

    UPROPERTY(EditAnywhere, Category="Transformation|Tags")
    FName MagnetTag = "Magnet";

    FVector GetNorthPoleWorldDir() const;
    FVector GetSouthPoleWorldDir() const;

private:
    void RefreshConnectedWires();
    void SetElectrified(bool bNewElectrified);
    void EnergizeWiresIfElectrified();

    void UpdateTagsForForm(EBlockForm Form);
    void ClearAllFormTags();
    void ApplySpec(const FBlockFormSpec& Spec);

    void EnterIceMode();
    void ExitIceMode();
    void RecalcIceMassAndEnergy();
    void ApplyIceMeltVisual(float Alpha01);

    void EnterWoodMode();
    void ExitWoodMode();
    void RecalcWoodMassAndVolume();
    void ApplyWoodBurnVisual(float Alpha01);

    void EnterMagnetMode();
    void ExitMagnetMode();
    void UpdateMagnetism(float DeltaTime);
    void RefreshOverlappingMetals();
    void DecreaseGaugeForCurrentTag();

    // ── AMagnet에서 가져온 누락 기능들 ──
    void CheckDemagnetize();
    void UpdateMagnetElectroBoost();
    void ApplyInducedMagnetism();
    float CalculateInducedStrength(float DistanceToMagnet, float BaseMagnetStrengthVal) const;

private:
    UPROPERTY(Transient)
    UMaterialInstanceDynamic* IceMID = nullptr;

    UPROPERTY(Transient)
    ATemperature* CurrentFire = nullptr;

    bool bHeating = false;
    float MeltAlpha = 0.0f;
    float EnergyAccumJ = 0.0f;
    float VolumeM3 = 1.0f;
    float EffectiveAreaM2 = 1.0f;
    float TotalMeltEnergyJ = 1.0f;
    FVector BaseScaleBeforeMelt = FVector(1.0f);

    UPROPERTY(Transient)
    UMaterialInstanceDynamic* BurnMID = nullptr;

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
    TArray<TObjectPtr<AWire>> MagnetContactedWires;

    float TimeSinceLastMagnetRefresh = 0.f;
    float BaseMagnetStrength = 0.f;

    /** 범위에 처음 들어온 금속을 추적하여 InitialImpulse 중복 방지 */
    UPROPERTY(Transient)
    TSet<TObjectPtr<UPrimitiveComponent>> PreviousOverlappingMetals;

    UFUNCTION()
    void OnMagnetHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

    bool bMagnetCollided = false;
    bool bMagnetSnapped = false;

    static constexpr float MaxForceClamp = 5e5f;
    static constexpr float MaxInducedForceClamp = 3e7f;
    static constexpr float GravityAccel = 980.f;

    // ── Arrow Effect (Magnet Visual) ──
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
};