#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Generator.generated.h"

class UStaticMeshComponent;
class AWire;
class AMagnet;

UCLASS()
class MATERIAL_API AGenerator : public AActor
{
    GENERATED_BODY()

public:
    AGenerator();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    // 고정 루트
    UPROPERTY(VisibleAnywhere, Category = "Generator")
    TObjectPtr<USceneComponent> Root;

    // 회전하는 메시
    UPROPERTY(VisibleAnywhere, Category = "Generator")
    TObjectPtr<UStaticMeshComponent> GeneratorMesh;

    // 자석 탐지 반경
    UPROPERTY(EditAnywhere, Category = "Generator|Magnet")
    float MagnetDetectRadius = 1500.f;

    // 코일 회전 속도 (deg/s)
    UPROPERTY(EditAnywhere, Category = "Generator|Coil")
    float RotationSpeed = 180.f;

    // 코일 권선수
    UPROPERTY(EditAnywhere, Category = "Generator|EMF")
    int32 CoilWindings = 100;

    // 코일 반지름 (cm)
    UPROPERTY(EditAnywhere, Category = "Generator|EMF")
    float CoilRadiusCM = 15.f;

    // EMF 최솟값 (이 이하면 전력 전달 안 함)
    UPROPERTY(EditAnywhere, Category = "Generator|EMF")
    float MinEMFThreshold = 0.1f;

    // Wire 탐지 반경
    UPROPERTY(EditAnywhere, Category = "Generator|Circuit")
    float WireDetectRadius = 200.f;

    // Wire 감지 위치 오프셋 (에디터에서 조정 가능)
    UPROPERTY(EditAnywhere, Category = "Generator|Circuit")
    FVector WireDetectOffset = FVector::ZeroVector;

    // 현재 EMF (디버그용)
    UPROPERTY(VisibleAnywhere, Category = "Generator|EMF")
    float CurrentEMF = 0.f;

    // 현재 회전각
    UPROPERTY(VisibleAnywhere, Category = "Generator|EMF")
    float RotationAngle = 0.f;

    // 전류 방향
    UPROPERTY(VisibleAnywhere, Category = "Generator|EMF")
    bool bCurrentPositive = true;

    UPROPERTY(EditAnywhere, Category = "Generator|Debug")
    bool bDebugDraw = true;

    // 감지된 자석들
    UPROPERTY()
    TObjectPtr<AMagnet> NorthMagnet;

    UPROPERTY()
    TObjectPtr<AMagnet> SouthMagnet;

    // 연결된 Wire들
    UPROPERTY()
    TArray<TObjectPtr<AWire>> ConnectedWires;

    void DetectMagnets();
    void UpdateEMF(float DeltaTime);
    void UpdateCircuit();
};