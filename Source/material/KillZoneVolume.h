#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "KillZoneVolume.generated.h"

UCLASS()
class YOURPROJECT_API AKillZoneVolume : public AActor
{
	GENERATED_BODY()

public:
	AKillZoneVolume();

protected:
	virtual void BeginPlay() override;

	// 충돌 박스
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KillZone")
	UBoxComponent* KillZoneBox;

	// 에디터에서 설정하는 리스폰 위치
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KillZone")
	FVector RespawnLocation;

	// 에디터에서 설정하는 리스폰 회전값 (선택)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KillZone")
	FRotator RespawnRotation;

	// 리스폰 전 딜레이 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KillZone", meta = (ClampMin = "0.0"))
	float RespawnDelay;

private:
	UFUNCTION()
	void OnOverlapBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	void RespawnPlayer(ACharacter* PlayerCharacter);
};