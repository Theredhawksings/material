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
    Idle,     // 대기
    Fire,     // 발사
    Cooldown  // 재장전 대기
};

UCLASS()
class MATERIAL_API ACoilGun : public AActor
{
    GENERATED_BODY()

public:
    ACoilGun();

    UFUNCTION(BlueprintCallable, Category = "CoilGun")
    ECoilGunState GetState() const { return CurrentState; }

    // Wire에서 전압 주입
    UFUNCTION(BlueprintCallable, Category = "CoilGun")
    void SetVoltage(float InVoltage) { CurrentVoltage = InVoltage; }

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

    // 철 감지 범위
    UPROPERTY(VisibleAnywhere, Category = "CoilGun")
    TObjectPtr<UBoxComponent> BarrelZone;

    // 발사 방향 (로컬 기준)
    // -Y: (0,-1,0) / -X: (-1,0,0) / +X: (1,0,0)
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    FVector FireDirection = FVector(0.f, -1.f, 0.f);

    // 전압 → 발사속도 변환 배율
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    float VoltageToSpeedMultiplier = 10.0f;

    // 전압 없을 때 기본 발사 속도 (테스트용)
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    float DefaultLaunchSpeed = 1000.f;

    // 최소 발사 속도
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    float MinLaunchSpeed = 300.f;

    // 최대 발사 속도
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    float MaxLaunchSpeed = 5000.f;

    // 철 감지 태그
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    FName IronTag = TEXT("Metal");

    // 쿨다운 시간
    UPROPERTY(EditAnywhere, Category = "CoilGun|Physics")
    float CooldownTime = 2.f;

    // Wire 감지 반경
    UPROPERTY(EditAnywhere, Category = "CoilGun|Circuit")
    float WireDetectRadius = 200.f;

    // 디버그
    UPROPERTY(EditAnywhere, Category = "CoilGun|Debug")
    bool bDebugDraw = true;

    // 현재 상태
    ECoilGunState CurrentState = ECoilGunState::Idle;

    // 현재 전압 (Wire에서 받아옴)
    UPROPERTY(VisibleAnywhere, Category = "CoilGun|Circuit")
    float CurrentVoltage = 0.f;

    // 쿨다운 타이머
    float CooldownTimer = 0.f;

    // 연결된 Wire
    UPROPERTY()
    TArray<TObjectPtr<AWire>> ConnectedWires;

    FVector GetFireWorldDir() const;
    void    DetectAndFire();
    void    DoFire(UPrimitiveComponent* IronComp);
    void    ReadVoltageFromWires();
    void    DebugVisualize();
};