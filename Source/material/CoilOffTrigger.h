#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "CoilOffTrigger.generated.h"

class ACoil;

UCLASS()
class MATERIAL_API ACoilOffTrigger : public AActor
{
	GENERATED_BODY()

public:
	ACoilOffTrigger();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, Category = "Trigger")
	TObjectPtr<UBoxComponent> TriggerBox;

	// 영구 정지시킬 코일들 (에디터에서 지정, 여러 개 가능)
	UPROPERTY(EditAnywhere, Category = "Trigger")
	TArray<TObjectPtr<ACoil>> TargetCoils;
};