#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MagnetCheckZone.generated.h"

class UBoxComponent;
class USoundBase; 

UCLASS()
class MATERIAL_API AMagnetCheckZone : public AActor
{
	GENERATED_BODY()

public:
	AMagnetCheckZone();

	UFUNCTION(BlueprintCallable, Category = "CheckZone")
	bool IsSatisfied() const { return bSatisfied; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, Category = "CheckZone")
	TObjectPtr<UBoxComponent> CheckBox;

	UPROPERTY(EditAnywhere, Category = "CheckZone")
	FVector CheckBoxExtent = FVector(60.f, 60.f, 60.f);

	UPROPERTY(EditAnywhere, Category = "CheckZone")
	FName MagnetTag = TEXT("Magnet");

	// ── 문 설정 (MainStage1_Platform1과 동일 방식) ──
	UPROPERTY(EditAnywhere, Category = "CheckZone|Door")
	TObjectPtr<AActor> LeftDoorActor;

	UPROPERTY(EditAnywhere, Category = "CheckZone|Door")
	TObjectPtr<AActor> RightDoorActor;

	UPROPERTY(EditAnywhere, Category = "CheckZone|Door")
	FVector OpenDirection = FVector(0.f, 1.f, 0.f);

	UPROPERTY(EditAnywhere, Category = "CheckZone|Door")
	float OpenDistance = 200.f;

	UPROPERTY(EditAnywhere, Category = "CheckZone|Door")
	float OpenSpeed = 1.f;

	UPROPERTY(EditAnywhere, Category = "CheckZone|Debug")
	bool bDebugDraw = true;

private:
	bool bSatisfied = false;
	bool bIsOpening = false;
	bool bIsOpen = false;
	float CurrentTime = 0.f;

	FVector LeftStartLocation;
	FVector LeftTargetLocation;
	FVector RightStartLocation;
	FVector RightTargetLocation;

	// ★ 문 열릴 때 재생할 효과음
	UPROPERTY(EditAnywhere, Category = "CheckZone|Door")
	TObjectPtr<USoundBase> DoorOpenSound;
};