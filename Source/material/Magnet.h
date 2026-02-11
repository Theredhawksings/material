// Magnet.h - 적당한 강도로 조정
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Magnet.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UPrimitiveComponent;

UCLASS()
class MATERIAL_API AMagnet : public AActor
{
    GENERATED_BODY()

public:
    AMagnet();
    virtual void Tick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;

    // Magnet.h
    UPROPERTY(VisibleAnywhere, Category="Magnet")
    UStaticMeshComponent* MagnetMesh;

    UPROPERTY(VisibleAnywhere, Category="Magnet")
    USphereComponent* MagnetRange;

    UPROPERTY(EditAnywhere, Category="Magnet")
    FName MetalTag = "Metal";

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float Strength;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float ReferenceDistance = 100.f;  // 80 -> 100

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float MaxLiftMass = 70.f;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float MinDistance = 10.f;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float MaxDistance = 800.f;  // 600 -> 800 (범위 증가)

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    bool bAutoComputeStrength = true;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float ForceMultiplier = 30.0f;  // 50 -> 30 (약하게)

    UPROPERTY()
    TSet<UPrimitiveComponent*> OverlappingMetals;

    UPROPERTY(EditAnywhere, Category = "Magnet")
    float MagneticDecayExponent = 1.5f;  // 1.3 -> 1.5 (거리 감쇠 증가)

    UPROPERTY(EditAnywhere, Category = "Magnet")
    float VelocityDampingFactor = 0.2f;  // 0.15 -> 0.2 (댐핑 증가)

    UPROPERTY(EditAnywhere, Category = "Magnet")
    float MaxAttractVelocity = 1500.f;  // 2000 -> 1500 (속도 제한)

    UPROPERTY(EditAnywhere, Category = "Magnet")
    bool bUseTorque = true;

    UPROPERTY(EditAnywhere, Category = "Magnet")
    bool bApplyInitialImpulse = false;

    UPROPERTY(EditAnywhere, Category = "Magnet")
    float InitialImpulseStrength = 200.f;

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bDebugDraw = true;

    UPROPERTY(EditAnywhere, Category = "Magnet|Advanced")
    float RefreshInterval = 0.1f;

    float TimeSinceLastRefresh = 0.f;
    // 기존 protected 섹션에 추가
    
    // 자기 유도 관련
    UPROPERTY(EditAnywhere, Category = "Magnet|Induction")
    bool bEnableInduction = true;

    UPROPERTY(EditAnywhere, Category = "Magnet|Induction")
    float InductionStrengthRatio = 0.3f;  // 유도 자석은 원본의 30% 힘

    UPROPERTY(EditAnywhere, Category = "Magnet|Induction")
    float InductionRange = 250.f;  // 유도 자석의 작용 범위

    UPROPERTY(EditAnywhere, Category = "Magnet|Induction")
    float MinDistanceForInduction = 200.f;  // 자석으로부터 이 거리 이내면 자화됨

private:
    // private 섹션에 추가
    void ApplyInducedMagnetism();
    float CalculateInducedStrength(float DistanceToMagnet, float BaseMagnetStrength) const;

public:
    UFUNCTION(BlueprintCallable, Category = "Magnet")
    void RefreshOverlappingMetals();

protected:
    UFUNCTION()
    void OnRangeBegin(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );

    UFUNCTION()
    void OnRangeEnd(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex
    );
};