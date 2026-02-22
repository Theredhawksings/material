#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Transformation_actor.generated.h"

class UStaticMeshComponent;
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
    Wood
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
    TArray<EBlockForm> CycleOrder = { EBlockForm::Metal, EBlockForm::Ice, EBlockForm::Rubber, EBlockForm::Wood };

    UFUNCTION(BlueprintCallable, Category="Form")
    void SetForm(EBlockForm NewForm);

    UFUNCTION(BlueprintCallable, Category="Form")
    void NextForm();

    UFUNCTION(BlueprintCallable, Category="Ice|Heating")
    void StartHeating(ATemperature* FireRef);

    UFUNCTION(BlueprintCallable, Category="Ice|Heating")
    void StopHeating();

    UFUNCTION(BlueprintCallable, Category="Heating")
    bool IsHeating() const { return bHeating; }

    EBlockForm GetCurrentForm() const { return CurrentForm; }
    const FBlockFormSpec* FindSpec(EBlockForm Form) const;

    // === Metal ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Metal|Electric")
    float WireSenseExtraRadius = 8.f;

    // === Ice ===
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

    // === Wood ===
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

    // === Tags ===
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

private:
    // Ice
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

    // Wood
    UPROPERTY(Transient)
    UMaterialInstanceDynamic* BurnMID = nullptr;

    float WoodTemperatureC = 20.0f;
    float WoodVolumeM3 = 1.0f;
    float WoodMassKg = 1.0f;
    float CurrentWoodMassKg = 1.0f;
    float BurnAlpha = 0.0f;
    bool bIsBurning = false;
    FVector BaseScaleBeforeBurn = FVector(1.0f);

    // Metal
    UPROPERTY(Transient)
    TArray<TObjectPtr<AWire>> ConnectedWires;

    bool bElectrified = false;
    TSet<TWeakObjectPtr<AWire>> WiresEnergizedByMetal;

    FTimerHandle RefreshTimerHandle;
};