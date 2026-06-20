#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MagnetJumpPad.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class ACharacter;

UCLASS()
class MATERIAL_API AMagnetJumpPad : public AActor
{
	GENERATED_BODY()

public:
	AMagnetJumpPad();
	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

	// ── 컴포넌트 ──────────────────────────────────────────
	UPROPERTY(VisibleAnywhere, Category = "MagnetJumpPad")
	USceneComponent* PadRoot;

	UPROPERTY(VisibleAnywhere, Category = "MagnetJumpPad")
	UStaticMeshComponent* BaseMesh;

	UPROPERTY(VisibleAnywhere, Category = "MagnetJumpPad")
	UStaticMeshComponent* PadMesh;

	UPROPERTY(VisibleAnywhere, Category = "MagnetJumpPad")
	USphereComponent* DetectRange;

	// ── 설정 ──────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Setup")
	float RestHeight = 0.f;

	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Setup")
	float TravelRange = 200.f;

	// ── 스프링 ────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Spring")
	float RestStiffness = 200.f;

	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Spring")
	float PlayerPressForce = 50000.f;

	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Spring")
	float Damping = 6.f;

	// ── 발사 ──────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Launch")
	float LaunchSpeed = 1200.f;

	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Launch")
	float MinLaunchSpeed = 400.f;

	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Launch")
	float MaxLaunchSpeed = 2000.f;

	// ── 감지 ──────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Detect")
	float PlayerDetectRadius = 120.f;

	// ── 디버그 ────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Debug")
	bool bDebugDraw = true;

private:
	// DetectRange 오버랩 콜백
	UFUNCTION()
	void OnDetectBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnDetectEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY()
	ACharacter* CachedPlayer = nullptr;

	float PadZ   = 0.f;
	float PadVel = 0.f;
	bool  bLaunched  = false;
	bool  bStepped   = false;   // DetectRange 오버랩 상태
};