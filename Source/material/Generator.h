#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Generator.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class AWire;
class AMagnet;
class USoundBase; 
class UAudioComponent;  
class USoundAttenuation;   // 전방 선언 추가

UCLASS()
class MATERIAL_API AGenerator : public AActor
{
    GENERATED_BODY()

public:
    AGenerator();

    UFUNCTION(BlueprintCallable, Category = "Generator")
    float GetCurrentEMF() const { return CurrentEMF; }

    UFUNCTION(BlueprintCallable, Category = "Generator")
    bool IsCurrentPositive() const { return bCurrentPositive; }

    UFUNCTION(BlueprintCallable, Category = "Generator")
    void ActivateGenerator()   { bGeneratorActive = true;  }

    UFUNCTION(BlueprintCallable, Category = "Generator")
    void DeactivateGenerator() { bGeneratorActive = false; }

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Generator")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, Category = "Generator")
    TObjectPtr<UStaticMeshComponent> GeneratorMesh;

    // 에디터에서 도넛 옆으로 위치 이동. 박스 안 전선에 전기 중계.
    UPROPERTY(VisibleAnywhere, Category = "Generator|OutputBox")
    TObjectPtr<UBoxComponent> OutputBox;

    // 체크박스 ON + 발전 중 + 정방향일 때 OutputBox가 전기를 중계
    UPROPERTY(EditAnywhere, Category = "Generator|OutputBox")
    bool bCoilOutputEnabled = true;

    // 발전기 본체에 붙어서 같이 회전하는 전선들
    UPROPERTY(EditAnywhere, Category = "Generator|Circuit")
    TArray<TObjectPtr<AWire>> AssignedWires;

    // true면 AssignedWires를 GeneratorMesh에 부착해 같이 회전
    UPROPERTY(EditAnywhere, Category = "Generator|Circuit")
    bool bRotateConnectedWires = true;

    UPROPERTY(EditAnywhere, Category = "Generator|Magnet")
    float MagnetDetectRadius = 1500.f;

    // 자석 1쌍일 때 기본 회전속도. 쌍이 늘수록 자동으로 줄어듦.
    UPROPERTY(EditAnywhere, Category = "Generator|Coil")
    float BaseRotationSpeed = 180.f;

    // 회전축 마스크 (기본 Yaw)
    UPROPERTY(EditAnywhere, Category = "Generator|Coil")
    FRotator SpinAxisMask = FRotator(0.f, 1.f, 0.f);

    UPROPERTY(EditAnywhere, Category = "Generator|EMF")
    int32 CoilWindings = 100;

    UPROPERTY(EditAnywhere, Category = "Generator|EMF")
    float CoilRadiusCM = 15.f;

    UPROPERTY(EditAnywhere, Category = "Generator|EMF")
    float MinEMFThreshold = 0.1f;

    // 불균형 페널티 강도 (0이면 페널티 없음, 1이면 최대 진동)
    UPROPERTY(EditAnywhere, Category = "Generator|EMF")
    float ImbalancePenaltyScale = 1.f;

    UPROPERTY(VisibleAnywhere, Category = "Generator|EMF")
    float CurrentEMF = 0.f;

    UPROPERTY(VisibleAnywhere, Category = "Generator|EMF")
    float RotationAngle = 0.f;

    UPROPERTY(VisibleAnywhere, Category = "Generator|EMF")
    float CurrentRotationSpeed = 180.f;

    UPROPERTY(VisibleAnywhere, Category = "Generator|EMF")
    int32 EffectivePairs = 0;

    UPROPERTY(VisibleAnywhere, Category = "Generator|EMF")
    float ImbalanceRatio = 0.f;

    UPROPERTY(VisibleAnywhere, Category = "Generator|EMF")
    bool bCurrentPositive = true;

    UPROPERTY(EditAnywhere, Category = "Generator|Debug")
    bool bDebugDraw = true;

    UPROPERTY()
    TArray<TObjectPtr<AMagnet>> NorthMagnets;

    UPROPERTY()
    TArray<TObjectPtr<AMagnet>> SouthMagnets;

    UPROPERTY()
    TArray<TObjectPtr<AWire>> BoxPoweredWires;

    float ImbalanceNoiseTime = 0.f;

    void DetectMagnets();
    void UpdateEMF(float DeltaTime);
    void UpdateCircuit();
    void UpdateGeneratorSound(); 

    UPROPERTY(EditAnywhere, Category = "Generator")
    bool bGeneratorActive = false;

    UPROPERTY(EditAnywhere, Category = "Generator|Sound")
    TObjectPtr<USoundBase> TurningOnSound;

    UPROPERTY(EditAnywhere, Category = "Generator|Sound")
    TObjectPtr<USoundBase> TurningOffSound;

    UPROPERTY(EditAnywhere, Category = "Generator|Sound")
    float GeneratorSoundDelay = 0.3f;

    bool bWasRunning = false;
    FTimerHandle GenSoundTimerHandle;

    // Generator|Magnet 카테고리에
UPROPERTY(EditAnywhere, Category = "Generator|Magnet")
float MagnetScanInterval = 0.2f;   // 0이면 매 프레임 스캔(원래 동작)

// ImbalanceNoiseTime 옆에
float MagnetScanAccumulator = 0.f;

UPROPERTY()
    TObjectPtr<UAudioComponent> ActiveGenAudio = nullptr;   // ★ 현재 재생 중인 사운드 핸들

    UPROPERTY(EditAnywhere, Category = "Generator|Sound")
    TObjectPtr<USoundAttenuation> GeneratorSoundAttenuation;
  
    UPROPERTY(EditAnywhere, Category = "Generator|Magnet")
    int32 MinRequiredPairs = 1;

};
