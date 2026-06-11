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

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coil", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> CoilMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coil", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> DetectionZone;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coil", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> MagneticFieldSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coil", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> BottomBlocker;

	// 인게임에서 실제 보이는 판 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coil", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BottomPlateMesh;

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

	UPROPERTY(BlueprintReadOnly, Category = "Coil|EMF", meta = (AllowPrivateAccess = "true"))
	float CurrentEMF = 0.f;

	UPROPERTY(EditAnywhere, Category = "Coil|EMF")
	int32 CoilWindings = 25;

	UPROPERTY(EditAnywhere, Category = "Coil|EMF")
	float MagnetFieldStrengthTesla = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Coil|EMF")
	float CoilInnerDiameterCM = 140.f;

	UPROPERTY(EditAnywhere, Category = "Coil|Debug")
	bool bDebugDraw = true;

	// 에디터/게임에서 디버그 셰이프(DetectionZone, MagneticFieldSphere, BottomBlocker) 보이기 토글
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coil|Debug", meta = (AllowPrivateAccess = "true"))
	bool bShowDebugShapes = true;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> DetectedMagnets;

	FVector BaseCoilLocation;
	float OscillationTime = 0.f;

	UPROPERTY(EditAnywhere, Category = "Coil|Circuit")
	float WireDetectRadius = 200.f;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> ConnectedWires;

	UPROPERTY(EditAnywhere, Category = "Coil|Induction")
	float InductionHeatingRate = 0.5f;

	void ApplyInductionHeating(float DeltaTime);
	void UpdateCircuit();
	void DetectMagnets();
	void ApplyOscillation(float DeltaTime);
	void ApplyMagneticForce();
	void UpdateFieldRadius();
	void DebugVisualize();
	void ApplyDebugVisibility();

	// 코일과 함께 움직일 액터들 (에디터에서 지정)
	UPROPERTY(EditAnywhere, Category = "Coil|Attached")
	TArray<TObjectPtr<AActor>> AttachedActors;

	// 각 액터의 코일 기준 상대 위치 저장용
	TArray<FVector> AttachedOffsets;	
};