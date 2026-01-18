// Temperature.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "Temperature.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;

UCLASS()
class MATERIAL_API ATemperature : public AActor
{
	GENERATED_BODY()

public:
	ATemperature();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	UFUNCTION(BlueprintCallable, Category="Heat")
	float GetTotalRadiantPowerW() const;

	UFUNCTION(BlueprintCallable, Category="Heat")
	float GetHeatFluxWm2AtLocation(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintCallable, Category="Heat")
	float GetHeatFluxWm2AtDistanceM(float DistanceM) const;

	UFUNCTION(BlueprintCallable, Category="Heat")
	float GetReceivedPowerW(const FVector& WorldLocation, float ReceiverAreaM2 = 1.0f) const;

private:
	UPROPERTY(VisibleAnywhere, Category="Components")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, Category="Components")
	UStaticMeshComponent* MeshComp;

	UPROPERTY(VisibleAnywhere, Category="Components")
	USphereComponent* HeatSphere;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Settings")
	float Temperature = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Settings")
	float MaxHeatDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Settings")
	float CoolRate = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Physics")
	float SurfaceAreaM2 = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Physics")
	float Emissivity = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Physics")
	float StefanBoltzmannSigma = 5.67e-8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Settings")
	TSubclassOf<AActor> IceClassFilter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Visual")
	bool bUseDynamicMaterial = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Visual")
	UMaterialInterface* HeatMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Visual")
	FName HeatAlphaParamName = TEXT("HeatAlpha");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Visual")
	float TempScale = 0.002f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Visual")
	bool bUseCPD = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heat|Visual")
	int32 CPDIndex_Temperature = 0;

private:
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* HeatMID = nullptr;

	float LastSphereRadius = -1.0f;

	UFUNCTION()
	void OnSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void OnSphereEndOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
	);

	void UpdateSphereRadius(bool bForceOverlaps);
	void StartHeatingOnAlreadyOverlapping();
	void UpdateVisuals();
	void CheckAndUpdateIceObjects();
};
