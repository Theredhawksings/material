#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GaugeResetZone.generated.h"

class UBoxComponent;

UCLASS()
class MATERIAL_API AGaugeResetZone : public AActor
{
	GENERATED_BODY()

public:
	AGaugeResetZone();

protected:
	UFUNCTION()
	void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

private:
	UPROPERTY(VisibleAnywhere, Category = "Zone")
	TObjectPtr<UBoxComponent> TriggerBox;

	// 한 번 발동되면 다시는 발동 안 함
	bool bHasTriggered = false;
};