#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoilGun.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class AWire;

UENUM()
enum class ECoilGunState : uint8
{
    Idle,      // 대기
    Charging,  // 전원 ON - 철 흡입중
    Fire,      // 전원 OFF - 발사
    Cooldown   // 재장전 대기
};

UCLASS()
class MATERIAL_API ACoilGun : public AActor
{
    GENERATED_BODY()

public:
    ACoilGun();

    UFUNCTION(BlueprintCallable, Category = "CoilGun")
    void OnTriggerPressed();

    UFUNCTION(BlueprintCallable, Category = "CoilGun")
    void OnTriggerReleased();

    UFUNCTION(BlueprintCallable, Category = "CoilGun")
    ECoilGunState GetState() const { return CurrentState; }

    UFUNCTION(BlueprintCallable, Category = "CoilGun")
    float GetPowerRatio() const { return PowerRatio; }

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

    // 인력 세기
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    float PullForce = 500000.f;

    // 발사 방향 (로컬 기준 - 에디터에서 자유롭게 설정)
    // 예) -Y: (0,-1,0) / -X: (-1,0,0) / +X: (1,0,0)
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    FVector FireDirection = FVector(0.f, -1.f, 0.f);

    // 코일 중심 도달 판정 거리
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    float CenterThreshold = 30.f;

    // 철 감지 태그
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    FName IronTag = TEXT("Metal");

    // 최소 발사 속도
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    float MinLaunchSpeed = 500.f;

    // 쿨다운 시간
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    float CooldownTime = 1.5f;

    // Wire 감지 반경
    UPROPERTY(EditAnywhere, Category = "CoilGun|Circuit")
    float WireDetectRadius = 200.f;

    // 디버그
    UPROPERTY(EditAnywhere, Category = "CoilGun|Debug")
    bool bDebugDraw = true;

    ECoilGunState CurrentState = ECoilGunState::Idle;

    UPROPERTY()
    TObjectPtr<UPrimitiveComponent> LoadedIron;

    UPROPERTY()
    TArray<TObjectPtr<AWire>> ConnectedWires;

    float PowerRatio    = 0.f;
    float CooldownTimer = 0.f;

    // 월드 기준 발사 방향 반환
    FVector GetFireWorldDir() const;

    void DetectIron();
    void ApplyPullForce();
    void CheckIronReachedCenter();
    void DoFire();
    void UpdatePowerRatio();
    void UpdateWireConnection(bool bPowered);
    void DebugVisualize();
};