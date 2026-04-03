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

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	// ── 컴포넌트 ──
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coil", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> CoilMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coil", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> DetectionZone;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coil", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> MagneticFieldSphere;

	// ── 감지 설정 ──
	UPROPERTY(EditAnywhere, Category = "Coil|Detection")
	FVector DetectionBoxExtent = FVector(60.f, 30.f, 30.f);

	UPROPERTY(EditAnywhere, Category = "Coil|Detection")
	FName MagnetTag = TEXT("Magnet");

	// ── 자기장 설정 ──
	UPROPERTY(EditAnywhere, Category = "Coil|MagneticField")
	float MagneticFieldRadius = 100.f;

	/** 자석 1개 추가될 때마다 자기장 반경 증가량 */
	UPROPERTY(EditAnywhere, Category = "Coil|MagneticField")
	float FieldRadiusPerMagnet = 50.f;

	/** 자석에 가하는 인력 세기 */
	UPROPERTY(EditAnywhere, Category = "Coil|MagneticField")
	float MagneticForceStrength = 500000.f;

	// ── 코일 진동 설정 ──
	UPROPERTY(EditAnywhere, Category = "Coil|Oscillation")
	float OscillationSpeed = 3.f;

	UPROPERTY(EditAnywhere, Category = "Coil|Oscillation")
	float OscillationAmplitude = 15.f;

	/** 자석 1개 추가될 때마다 진동 속도 증가량 */
	UPROPERTY(EditAnywhere, Category = "Coil|Oscillation")
	float SpeedPerExtraMagnet = 1.5f;

	// ── 디버그 ──
	UPROPERTY(EditAnywhere, Category = "Coil|Debug")
	bool bDebugDraw = true;

	// ── 내부 상태 ──
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> DetectedMagnets;

	FVector BaseCoilLocation;
	float OscillationTime = 0.f;

	// ── 내부 함수 ──
	void DetectMagnets();
	void ApplyOscillation(float DeltaTime);
	void ApplyMagneticForce();
	void UpdateFieldRadius();
	void DebugVisualize();
};