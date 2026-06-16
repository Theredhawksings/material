#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Magnet.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UPrimitiveComponent;
class ATemperature;
class AWire;

UCLASS()
class MATERIAL_API AMagnet : public AActor
{
    GENERATED_BODY()

public:
    AMagnet();
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "Magnet")
    void RefreshOverlappingMetals();

    UFUNCTION(BlueprintCallable, Category = "Magnet|Visual")
    static void SetAllArrowsVisible(bool bVisible);

    // ★ 추가: N/S 극 방향 반환
    UFUNCTION(BlueprintCallable, Category = "Magnet|Polarity")
    FVector GetNorthPoleWorldDir() const;

    UFUNCTION(BlueprintCallable, Category = "Magnet|Polarity")
    FVector GetSouthPoleWorldDir() const;

    // ★ 추가: Generator가 읽을 수 있도록 public으로
    UFUNCTION(BlueprintCallable, Category = "Magnet|Polarity")
    bool IsNorthPole() const { return bIsNorthPole; }

    float GetStrength() const { return Strength; }
    float GetDecayExponent() const { return MagneticDecayExponent; }  
    float GetReferenceDistance() const { return ReferenceDistance; } 
    bool IsDemagnetized() const { return bDemagnetized; }

    UFUNCTION(BlueprintCallable, Category = "Magnet|Camera", meta = (WorldContext = "WorldContextObject"))
    static void SetGlobalMagnetCameraState(const UObject* WorldContextObject, bool bIsCameraOn);

    void SetMagneticCameraState(bool bIsCameraOn);   // 개별 자석용

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category = "Magnet")
    TObjectPtr<UStaticMeshComponent> MagnetMesh;

    UPROPERTY(VisibleAnywhere, Category = "Magnet")
    TObjectPtr<USphereComponent> MagnetRange;

    UPROPERTY(VisibleAnywhere, Category = "Magnet|Electro")
    TObjectPtr<USphereComponent> WireContactRange;

    UPROPERTY(EditAnywhere, Category = "Magnet")
    FName MetalTag = "Metal";

    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float Strength = 0.f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float ReferenceDistance = 100.f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float MaxLiftMass = 30.f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float MinDistance = 10.f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float MaxDistance = 800.f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    bool bAutoComputeStrength = true;

    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float ForceMultiplier = 20.0f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float MagneticDecayExponent = 1.5f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float VelocityDampingFactor = 0.2f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float MaxAttractVelocity = 1500.f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    bool bUseTorque = true;

    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    bool bApplyInitialImpulse = false;

    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float InitialImpulseStrength = 200.f;

    // ★ 추가: 회전 방향
    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    bool bRotateClockwise = true;

    UPROPERTY(EditAnywhere, Category = "Magnet|Physics")
    float RotationSpeed = 0.f; // 0이면 회전 안 함

    UPROPERTY(EditAnywhere, Category = "Magnet|Induction")
    bool bEnableInduction = true;

    UPROPERTY(EditAnywhere, Category = "Magnet|Induction")
    float InductionStrengthRatio = 0.3f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Induction")
    float InductionRange = 250.f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Induction")
    float MinDistanceForInduction = 200.f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Electro")
    float WireContactRadius = 80.f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Electro")
    float ElectroBoostMultiplier = 3.0f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Advanced")
    float RefreshInterval = 0.1f;

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bDebugDraw = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Magnet|Curie")
    bool bDemagnetized = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Magnet|Electro")
    bool bElectroActive = false;

    // ★ 추가: N/S 극 설정
    UPROPERTY(EditAnywhere, Category = "Magnet|Polarity")
    bool bIsNorthPole = true;

    UPROPERTY(EditAnywhere, Category = "Magnet|Polarity")
    FVector NorthPoleLocalDir = FVector(1.f, 0.f, 0.f);

    UPROPERTY(EditAnywhere, Category = "Magnet|Visual")
    TSubclassOf<AActor> ArrowEffectClass;

    UPROPERTY(EditAnywhere, Category = "Magnet|Visual")
    float ArrowPower = 5.0f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Visual")
    float ArrowX = 100.0f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Visual")
    float ArrowY = 100.0f;

    UPROPERTY(EditAnywhere, Category = "Magnet|Visual")
    bool bShowFieldArrows = true;

    UFUNCTION()
    void OnRangeBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnRangeEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    UFUNCTION()
    void OnWireContactBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnWireContactEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    UPROPERTY()
    TSet<TObjectPtr<UPrimitiveComponent>> OverlappingMetals;

    UPROPERTY()
    TArray<TObjectPtr<AWire>> ContactedWires;

    UPROPERTY()
    TObjectPtr<AActor> SpawnedArrowEffect;

    float TimeSinceLastRefresh = 0.f;
    float BaseStrength = 0.f;

    // ★ 추가: Arrow 스폰 함수 분리
    void SpawnArrowEffect();
    void SyncArrowTransform();
    void UpdateArrowVisibility();

    void UpdateElectroBoost();
    void ApplyInducedMagnetism();
    float CalculateInducedStrength(float DistanceToMagnet, float BaseMagnetStrength) const;
    void CheckDemagnetize();

    static constexpr float MaxForceClamp = 6e7f;
    static constexpr float MaxInducedForceClamp = 3e7f;
    static constexpr float GravityAccel = 980.f;

    bool bIsMagneticCameraOn = false;

    void RefreshArrowVisibility();

    UPROPERTY(EditAnywhere, Category = "Magnet|Visual")
    float ArrowScaleMul = 0.2f;   // 화살표 크기 (작을수록 작아짐)

    UPROPERTY(EditAnywhere, Category = "Magnet|Visual", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float ArrowDownRatio = 1.0f;  
};