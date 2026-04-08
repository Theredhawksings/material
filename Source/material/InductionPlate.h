#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InductionPlate.generated.h"

class UStaticMeshComponent;

UCLASS()
class MATERIAL_API AInductionPlate : public AActor
{
	GENERATED_BODY()

public:
	AInductionPlate();

	UFUNCTION(BlueprintCallable, Category = "InductionPlate")
	float GetTemperature() const { return TemperatureC; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "InductionPlate", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PlateMesh;

	UPROPERTY(BlueprintReadOnly, Category = "InductionPlate", meta = (AllowPrivateAccess = "true"))
	float TemperatureC = 20.f;

	UPROPERTY(EditAnywhere, Category = "InductionPlate|Debug")
	bool bDebugDraw = true;
};