#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InductionPlate.generated.h"

class UStaticMeshComponent;
class ATransformation_actor;

UCLASS()
class MATERIAL_API AInductionPlate : public AActor
{
	GENERATED_BODY()

public:
	AInductionPlate();

	void ReceiveInductionHeat(float EnergyJ);

	float GetTemperature() const { return TemperatureC; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, Category = "Plate")
	TObjectPtr<UStaticMeshComponent> PlateMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plate", meta = (AllowPrivateAccess = "true"))
	float TemperatureC = 20.f;

	UPROPERTY(EditAnywhere, Category = "Plate|HeatTransfer")
	float HeatTransferRadius = 100.f;

	UPROPERTY(EditAnywhere, Category = "Plate|HeatTransfer")
	float HeatTransferRate = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Plate|Debug")
	bool bDebugDraw = true;
};