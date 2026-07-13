#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoilGun.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class AWire;
class AGenerator;

UENUM()
enum class ECoilGunState : uint8
{
    Idle,      // 대기
    Charging,  // 흡입중 (sin > 0)
    Fired,     // 발사됨 (sin < 0)
    Cooldown   // 재장전 대기
};

UCLASS()
class MATERIAL_API ACoilGun : public AActor
{
    GENERATED_BODY()

public:
    ACoilGun();

    UFUNCTION(BlueprintCallable, Category = "CoilGun")
    ECoilGunState GetState() const { return CurrentState; }

    UFUNCTION(BlueprintCallable, Category = "CoilGun")
    float GetVoltage() const { return CurrentVoltage; }

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY(VisibleAnywhere, Category = "CoilGun")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, Category = "CoilGun")
    TObjectPtr<UStaticMeshComponent> CoilMesh;

    UPROPERTY(VisibleAnywhere, Category = "CoilGun")
    TObjectPtr<UBoxComponent> BarrelZone;

    // ★ 발전기 직접 참조 (에디터에서 지정)
    UPROPERTY(EditAnywhere, Category = "CoilGun|Circuit")
    TObjectPtr<AGenerator> ConnectedGenerator;

    // 발사 방향 (로컬 기준)
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    FVector FireDirection = FVector(0.f, -1.f, 0.f);

    // 코일 권선수
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    int32 CoilWindings = 50;

    // 코일 반지름 (cm)
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    float CoilRadiusCM = 10.f;

    // 코일 길이 (cm)
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    float CoilLengthCM = 20.f;

    // 코일 저항 (Ω)
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    float CoilResistance = 1.f;

    // 게임 스케일 배율
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    float ForceScaleMultiplier = 1.f;

    // 최대 흡입 속도
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    float MaxPullSpeed = 300.f;

    // 발사 속도 (cm/s) — 흡입 속도가 이보다 크면 흡입 속도로 발사
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    float LaunchSpeed = 2000.f;

    // 발사 트리거 거리 (cm) — 철이 코일 중심에서 이 거리 안에 오면 발사
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    float FireTriggerDistance = 10.f;

    // 최대 흡입 시간 (초) — 이 시간 안에 중심에 못 오면 그냥 발사 (무한 대기 방지)
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    float MaxChargeTime = 5.f;

    // 쿨다운 시간
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    float CooldownTime = 1.f;

    // 철 감지 태그
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    FName IronTag = TEXT("Metal");

    // Wire 감지 반경
    UPROPERTY(EditAnywhere, Category = "CoilGun|Circuit")
    float WireDetectRadius = 200.f;

    // 디버그
    UPROPERTY(EditAnywhere, Category = "CoilGun|Debug")
    bool bDebugDraw = true;

    ECoilGunState CurrentState = ECoilGunState::Idle;

    UPROPERTY(VisibleAnywhere, Category = "CoilGun|Circuit")
    float CurrentVoltage = 0.f;

    UPROPERTY(VisibleAnywhere, Category = "CoilGun|Circuit")
    bool bCurrentPositive = false;

    UPROPERTY()
    TObjectPtr<UPrimitiveComponent> LoadedIron;

    UPROPERTY()
    TArray<TObjectPtr<AWire>> ConnectedWires;

    float CooldownTimer = 0.f;
    float ChargeTimer = 0.f;

    FVector GetFireWorldDir() const;
    void    ReadWireState();
    void    DetectIron();
    void    ApplyMagneticForce();
    void    ReleaseFire();
    void    DebugVisualize();

    // ── 열화상(Stencil) 효과 ──
    // Stencil 활성화할 메시 (장전된 철 또는 코일 메시)
    UPROPERTY(EditAnywhere, Category = "CoilGun|Thermal")
    bool bEnableThermalStencil = true;

    // 발사 가능으로 판단할 최소 힘 (N, ForceScale 적용 전 기준)
    UPROPERTY(EditAnywhere, Category = "CoilGun|Thermal")
    float ThermalForceThreshold = 0.0001f;

    // 식는 속도 (초당 stencil 감소량)
    UPROPERTY(EditAnywhere, Category = "CoilGun|Thermal")
    float ThermalCoolRate = 80.f;

    // 데우는 속도 (초당 stencil 증가량)
    UPROPERTY(EditAnywhere, Category = "CoilGun|Thermal")
    float ThermalHeatRate = 400.f;

    // 현재 stencil 값 (0~255, float로 보간)
    float CurrentStencil = 0.f;

    // stencil 적용 대상
    UPROPERTY()
    TObjectPtr<UPrimitiveComponent> ThermalTarget;

    void UpdateThermalStencil(float DeltaTime, bool bCoilHeating, bool bIronHeating);

    // 열화상 스텐실 - 대상별 온도값
float CoilStencil = 0.f;   // 코일 메시 온도
float IronStencil = 0.f;   // 장전된 철 온도

// 발사된 철(잔열 띠고 날아가는 탄) 추적
TWeakObjectPtr<UPrimitiveComponent> FiredIron;
float FiredIronStencil = 0.f;
};