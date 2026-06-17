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

	UPROPERTY(VisibleAnywhere, Category = "MagnetJumpPad")
	USceneComponent* PadRoot;

	UPROPERTY(VisibleAnywhere, Category = "MagnetJumpPad")
	UStaticMeshComponent* BaseMesh;

	UPROPERTY(VisibleAnywhere, Category = "MagnetJumpPad")
	UStaticMeshComponent* PadMesh;

	UPROPERTY(VisibleAnywhere, Category = "MagnetJumpPad")
	USphereComponent* DetectRange;

	// 발판 평형 높이 (0 = 베이스와 같은 위치)
	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Setup")
	float RestHeight = 0.f;

	// 최대로 눌릴 수 있는 깊이
	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Setup")
	float TravelRange = 200.f;

	// 복원 강성
	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Spring")
	float RestStiffness = 200.f;

	// 밟는 힘
	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Spring")
	float PlayerPressForce = 50000.f;

	// 감쇠
	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Spring")
	float Damping = 6.f;

	// 발사 속도
	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Launch")
	float LaunchSpeed = 1200.f;

	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Launch")
	float MinLaunchSpeed = 400.f;

	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Launch")
	float MaxLaunchSpeed = 2000.f;

	// 플레이어 감지
	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Detect")
	float PlayerDetectRadius = 120.f;

	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Detect")
	float PlayerDetectHeight = 200.f;

	UPROPERTY(EditAnywhere, Category = "MagnetJumpPad|Debug")
	bool bDebugDraw = true;

private:
	UPROPERTY()
	ACharacter* CachedPlayer = nullptr;

	float PadZ = 0.f;
	float PadVel = 0.f;
	bool bLaunched = false;

	bool IsPlayerOnPad() const;
};