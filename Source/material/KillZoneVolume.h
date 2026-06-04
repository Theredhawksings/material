#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KillZoneVolume.generated.h"

UCLASS()
class MATERIAL_API AKillZoneVolume : public AActor
{
	GENERATED_BODY()

public:
	AKillZoneVolume();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KillZone")
	UBoxComponent* KillZoneBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KillZone")
	FVector RespawnLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KillZone")
	FRotator RespawnRotation;

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