#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

	UFUNCTION(BlueprintCallable, Category = "Heat")
	float GetTotalRadiantPowerW() const;

	UFUNCTION(BlueprintCallable, Category = "Heat")
	float GetHeatFluxWm2AtLocation(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintCallable, Category = "Heat")
	float GetHeatFluxWm2AtDistanceM(float DistanceM) const;

	UFUNCTION(BlueprintCallable, Category = "Heat")
	float GetReceivedPowerW(const FVector& WorldLocation, float ReceiverAreaM2 = 1.0f) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat|Settings")
	float Temperature = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat|Settings")
	float MaxHeatDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat|Settings")
	float CoolRate = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat|Physics")
	float SurfaceAreaM2 = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat|Physics")
	float Emissivity = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat|Settings")
	TSubclassOf<AActor> IceClassFilter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat|Visual")
	bool bUseDynamicMaterial = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat|Visual")
	TObjectPtr<UMaterialInterface> HeatMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat|Visual")
	FName HeatAlphaParamName = TEXT("HeatAlpha");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat|Visual")
	float TempScale = 0.002f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat|Visual")
	bool bUseCPD = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat|Visual")
	int32 CPDIndex_Temperature = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat|Visual")
	bool bUseStencil = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat|Visual", meta = (EditCondition = "bUseStencil"))
	float MaxStencilTemperature = 2000.0f;

	// 에디터/게임에서 디버그 셰이프(HeatSphere) 보이기 토글
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat|Debug")
	bool bShowDebugShapes = true;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USphereComponent> HeatSphere;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HeatMID = nullptr;

	float LastSphereRadius = -1.0f;

	static constexpr float StefanBoltzmannSigma = 5.67e-8f;

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void UpdateSphereRadius(bool bForceOverlaps);
	void UpdateVisuals();
	void EnsureOverlappingActorsHeating();
	void CallFunctionOnActor(AActor* Target, FName FunctionName, void* Params = nullptr) const;
	void GetFilteredOverlappingActors(TArray<AActor*>& OutActors) const;
	void ApplyDebugVisibility();
};