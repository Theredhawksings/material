#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Coil.generated.h"

class UStaticMeshComponent;
class UBoxComponent;

UCLASS()
class MATERIAL_API ACoil : public AActor
{
	GENERATED_BODY()

public:
	ACoil();

	UFUNCTION(BlueprintCallable, Category = "Coil")
	bool HasMagnetInside() const { return DetectedMagnets.Num() > 0; }

	UFUNCTION(BlueprintCallable, Category = "Coil")
	int32 GetMagnetCount() const { return DetectedMagnets.Num(); }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coil", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> CoilMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coil", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> DetectionZone;

	UPROPERTY(EditAnywhere, Category = "Coil|Detection")
	FVector DetectionBoxExtent = FVector(60.f, 30.f, 30.f);

	UPROPERTY(EditAnywhere, Category = "Coil|Detection")
	FName MagnetTag = TEXT("Magnet");

	UPROPERTY(EditAnywhere, Category = "Coil|Debug")
	bool bDebugDraw = true;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> DetectedMagnets;
};