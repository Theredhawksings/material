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

	// 전선 끝점이 이 반경 안에 있어야 "연결됨"으로 인정
	UPROPERTY(EditAnywhere, Category = "Plate|HeatTransfer")
	float WireConnectRadius = 80.f;

	// 전력(V*I) → 온도 변환 비율 (낮게)
	UPROPERTY(EditAnywhere, Category = "Plate|HeatTransfer")
	float WireHeatingRate = 0.2f;

	// 플레이트가 한 프레임에 오를 최대 온도 (급상승 방지)
	UPROPERTY(EditAnywhere, Category = "Plate|HeatTransfer")
	float PlateMaxRisePerCall = 1.0f;

	// 자석/철이 한 프레임에 오를 최대 온도
	UPROPERTY(EditAnywhere, Category = "Plate|HeatTransfer")
	float BlockMaxRisePerCall = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Plate|HeatTransfer")
	float PlateCoolingRatePerSec = 30.f;
};