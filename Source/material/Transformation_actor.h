#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Transformation_actor.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPhysicalMaterial;
class ATemperature;

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

	UFUNCTION(BlueprintCallable, Category="Heat")
	void StartHeating(ATemperature* FireRef);

	UFUNCTION(BlueprintCallable, Category="Heat")
	void StopHeating();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Visual")
	UMaterialInterface* IceMeltMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Visual")
	FName MeltParamName = TEXT("MeltAlpha");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Melt")
	float MinScaleRatio = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Melt")
	bool bDestroyWhenMelted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Physics")
	float IceDensityKgM3 = 917.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Physics")
	float LatentHeatJPerKg = 334000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Physics")
	float SimTimeScale = 3600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Debug")
	bool bDebugMelt = true;

private:
	const FBlockFormSpec* FindSpec(EBlockForm Form) const;
	void ApplySpec(const FBlockFormSpec& Spec);

	void EnterIceMode();
	void ExitIceMode();

	void RecalcIceMassAndEnergy();
	void ApplyIceMeltVisual(float Alpha01);

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
	float DebugAcc = 0.0f;
};
