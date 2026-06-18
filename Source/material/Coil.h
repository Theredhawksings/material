#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Coil.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USoundBase;        // ★ 추가
class UAudioComponent;   // ★ 추가

UCLASS()
class MATERIAL_API ACoil : public AActor
{
	GENERATED_BODY()

public:
	ACoil();

	UFUNCTION(BlueprintCallable, Category = "Coil")
	bool HasMagnetInside() const { return MagnetsInside.Num() > 0; }

	UFUNCTION(BlueprintCallable, Category = "Coil")
	int32 GetMagnetCount() const { return MagnetsInside.Num(); }

	UFUNCTION(BlueprintCallable, Category = "Coil")
	bool IsGenerating() const { return CurrentEMF > 0.f; }

	UFUNCTION(BlueprintCallable, Category = "Coil")
	bool IsCoilActive() const { return bCoilActive; }

	UFUNCTION(BlueprintCallable, Category = "Coil")
	void SetCoilActive(bool bNewActive);

	UFUNCTION(BlueprintCallable, Category = "Coil")
	void ShutdownCoil();

	UFUNCTION(BlueprintCallable, Category = "Coil")
	bool IsShutdown() const { return bShutdown; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	// ── 컴포넌트 ──
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coil", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> CoilMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coil", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> DetectionZone;

	// ── 자석 감지 ──
	UPROPERTY(EditAnywhere, Category = "Coil|Detection")
	FVector DetectionBoxExtent = FVector(60.f, 30.f, 30.f);

	UPROPERTY(EditAnywhere, Category = "Coil|Detection")
	FName MagnetTag = TEXT("Magnet");

	// 코일이 자석 당기는 힘
	UPROPERTY(EditAnywhere, Category = "Coil|MagneticForce")
	float MagneticForceStrength = 500000.f;

	// ── EMF 충전/감쇠 (왕복 발전) ──
	UPROPERTY(EditAnywhere, Category = "Coil|EMF")
	float EMFPerSwing = 10.f;      // 자석 1회 뺄 때 충전량

	UPROPERTY(EditAnywhere, Category = "Coil|EMF")
	float EMFDecayRate = 10.f;    // 초당 감쇠량 (멈추면 빨리 식음)

	UPROPERTY(EditAnywhere, Category = "Coil|EMF")
	float MaxEMF = 50.f;          // 최대 상한

	UPROPERTY(BlueprintReadOnly, Category = "Coil|EMF", meta = (AllowPrivateAccess = "true"))
	float CurrentEMF = 0.f;

	// ── 전선 연결 ──
	UPROPERTY(EditAnywhere, Category = "Coil|Circuit")
	float WireDetectRadius = 200.f;

	// ── 전선 사운드 (전기 나오는 동안 재생) ──
	UPROPERTY(EditAnywhere, Category = "Coil|Sound")
	TObjectPtr<USoundBase> WireSound;

	UPROPERTY()
	TObjectPtr<UAudioComponent> WireAudioComp;

	// ── 디버그 ──
	UPROPERTY(EditAnywhere, Category = "Coil|Debug")
	bool bDebugDraw = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coil|Debug", meta = (AllowPrivateAccess = "true"))
	bool bShowDebugShapes = true;

	// ── 상태 ──
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coil", meta = (AllowPrivateAccess = "true"))
	bool bCoilActive = true;

	bool bShutdown = false;

	// 현재 박스 안에 있는 자석들 (당기는 힘/카운트용)
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> MagnetsInside;

	// 진입/이탈 추적: 직전 프레임에 박스 안에 있던 자석들
	TSet<TWeakObjectPtr<AActor>> MagnetsInsideLastFrame;

	// 코일에 전기 받는 전선들
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> ConnectedWires;

	FVector BaseCoilLocation;

	// ── 함수 ──
	void UpdateMagnetSensing();
	bool IsActorInsideZone(AActor* Actor) const;
	void ApplyMagneticForce();
	void UpdateCircuit();
	void ShutdownConnectedWires();
	void UpdateWireSound();   // ★ 추가
	void DebugVisualize();
	void ApplyDebugVisibility();
};