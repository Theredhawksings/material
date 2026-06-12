#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MagnetCheckZone.generated.h"

class UBoxComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMagnetCheckPassed);

UCLASS()
class MATERIAL_API AMagnetCheckZone : public AActor
{
	GENERATED_BODY()

public:
	AMagnetCheckZone();

	// 문 블루프린트에서 이 이벤트에 바인딩 → 문 열기 (한 번만 발사됨)
	UPROPERTY(BlueprintAssignable, Category = "CheckZone")
	FOnMagnetCheckPassed OnCheckPassed;

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

	UPROPERTY(EditAnywhere, Category = "CheckZone|Debug")
	bool bDebugDraw = true;

	bool bSatisfied = false;
};