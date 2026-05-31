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

    FVector GetFireWorldDir() const;
    void    ReadWireState();
    void    DetectIron();
    void    ApplyMagneticForce();
    void    ReleaseFire();
    void    DebugVisualize();
};