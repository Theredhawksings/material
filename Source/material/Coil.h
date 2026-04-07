#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Coil.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USphereComponent;

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

	UFUNCTION(BlueprintCallable, Category = "Coil")
	void ToggleCoil();

	UFUNCTION(BlueprintCallable, Category = "Coil")
	bool IsCoilActive() const { return bCoilActive; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coil", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> CoilMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coil", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> DetectionZone;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coil", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> MagneticFieldSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coil", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> BottomBlocker;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coil", meta = (AllowPrivateAccess = "true"))
	bool bCoilActive = true;

	UPROPERTY(EditAnywhere, Category = "Coil|Detection")
	FVector DetectionBoxExtent = FVector(60.f, 30.f, 30.f);

	UPROPERTY(EditAnywhere, Category = "Coil|Detection")
	FName MagnetTag = TEXT("Magnet");

	UPROPERTY(EditAnywhere, Category = "Coil|MagneticField")
	float MagneticFieldRadius = 100.f;

	UPROPERTY(EditAnywhere, Category = "Coil|MagneticField")
	float FieldRadiusPerMagnet = 50.f;

	UPROPERTY(EditAnywhere, Category = "Coil|MagneticField")
	float MagneticForceStrength = 500000.f;

	UPROPERTY(EditAnywhere, Category = "Coil|Oscillation")
	float OscillationSpeed = 3.f;

	UPROPERTY(EditAnywhere, Category = "Coil|Oscillation")
	float OscillationAmplitude = 15.f;

	UPROPERTY(EditAnywhere, Category = "Coil|Oscillation")
	float SpeedPerExtraMagnet = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Coil|Debug")
	bool bDebugDraw = true;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> DetectedMagnets;

	FVector BaseCoilLocation;
	float OscillationTime = 0.f;

	void DetectMagnets();
	void ApplyOscillation(float DeltaTime);
	void ApplyMagneticForce();
	void UpdateFieldRadius();
	void DebugVisualize();
};