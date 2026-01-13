// Ice.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ice.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class ATemperature;

UCLASS()
class MATERIAL_API AIce : public AActor
{
	GENERATED_BODY()

public:
	AIce();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	UFUNCTION(BlueprintCallable, Category="Ice")
	void StartHeating(ATemperature* FireRef);

	UFUNCTION(BlueprintCallable, Category="Ice")
	void StopHeating();

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ice|Components")
	UStaticMeshComponent* MeshComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Visual")
	UMaterialInterface* IceMeltMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Visual")
	FName MeltParamName = TEXT("MeltAlpha");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Melt")
	float MinScaleRatio = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Melt")
	bool bDestroyMeshWhenMelted = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Physics")
	float IceDensityKgM3 = 917.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Physics")
	float LatentHeatJPerKg = 334000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Physics")
	float SimTimeScale = 3600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ice|Debug")
	bool bDebugMelt = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ice|State")
	bool bHeating = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ice|State")
	float MeltAlpha = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ice|State")
	float EnergyAccumJ = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ice|State")
	FVector InitialScale = FVector(1.0f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ice|State")
	ATemperature* CurrentFire = nullptr;

private:
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* IceMI = nullptr;

	float VolumeM3 = 1.0f;
	float EffectiveAreaM2 = 1.0f;
	float TotalMeltEnergyJ = 1.0f;
	float DebugAcc = 0.0f;

	void RecalcMassAndEnergy();
	void ApplyMeltVisual(float Alpha01);
};
