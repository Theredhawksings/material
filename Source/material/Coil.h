#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Coil.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USoundBase;
class UAudioComponent;
class UMaterialInstanceDynamic; // ★ 추가

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
    float EMFPerSwing = 10.f;

    UPROPERTY(EditAnywhere, Category = "Coil|EMF")
    float EMFDecayRate = 10.f;

    UPROPERTY(EditAnywhere, Category = "Coil|EMF")
    float MaxEMF = 50.f;

    UPROPERTY(BlueprintReadOnly, Category = "Coil|EMF", meta = (AllowPrivateAccess = "true"))
    float CurrentEMF = 0.f;

    // ── 전선 연결 ──
    UPROPERTY(EditAnywhere, Category = "Coil|Circuit")
    float WireDetectRadius = 200.f;

    // ── 전선 사운드 ──
    UPROPERTY(EditAnywhere, Category = "Coil|Sound")
    TObjectPtr<USoundBase> WireSound;

    UPROPERTY()
    TObjectPtr<UAudioComponent> WireAudioComp;

    // ── ★ 시각 효과 (온도 및 스텐실) ──
    UPROPERTY(EditAnywhere, Category = "Coil|Effects")
    FName MaterialParameterName = TEXT("Temperature"); // 머티리얼 내의 파라미터 이름

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

    // ── 디버그 ──
    UPROPERTY(EditAnywhere, Category = "Coil|Debug")
    bool bDebugDraw = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coil|Debug", meta = (AllowPrivateAccess = "true"))
    bool bShowDebugShapes = true;

    // ── 상태 ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coil", meta = (AllowPrivateAccess = "true"))
    bool bCoilActive = true;

    bool bShutdown = false;

    UPROPERTY()
    TArray<TWeakObjectPtr<AActor>> MagnetsInside;

    TSet<TWeakObjectPtr<AActor>> MagnetsInsideLastFrame;

    UPROPERTY()
    TArray<TWeakObjectPtr<AActor>> ConnectedWires;

    FVector BaseCoilLocation;

    // ── 함수 ──
    void UpdateMagnetSensing();
    bool IsActorInsideZone(AActor* Actor) const;
    void ApplyMagneticForce();
    void UpdateCircuit();
    void ShutdownConnectedWires();
    void UpdateWireSound();
    void UpdateVisualEffects(float DeltaTime);
    void DebugVisualize();
    void ApplyDebugVisibility();

	// ── 열화상 스텐실 ──
UPROPERTY(EditAnywhere, Category = "Coil|Effects")
bool bEnableThermalStencil = true;

UPROPERTY(EditAnywhere, Category = "Coil|Effects")
float ThermalHeatRate = 120.f;   // 초당 가열 속도

UPROPERTY(EditAnywhere, Category = "Coil|Effects")
float ThermalCoolRate = 60.f;    // 초당 냉각 속도

float CoilStencil = 0.f;          // 현재 코일 온도(0~255)
};