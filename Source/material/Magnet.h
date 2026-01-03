// Magnet.h
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

    /* ===== Components ===== */

    /** 자석 본체 */
    UPROPERTY(VisibleAnywhere, Category="Magnet")
    UStaticMeshComponent* MagnetMesh;

    /** 자기장 범위 */
    UPROPERTY(VisibleAnywhere, Category="Magnet")
    USphereComponent* MagnetRange;

    /* ===== Gameplay Params ===== */

    /** 금속 판정 태그 */
    UPROPERTY(EditAnywhere, Category="Magnet")
    FName MetalTag = "Metal";

    /** 힘 계수 (자동 계산됨) */
    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float Strength;

    /** 기준 거리 r0 (cm) */
    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float ReferenceDistance = 300.f;

    /** 들어올릴 수 있는 최대 질량 (kg) */
    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float MaxLiftMass = 500.f;

    /** 최소 거리 (폭주 방지) */
    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float MinDistance = 50.f;

    /** 최대 거리 (자기장 범위) */
    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float MaxDistance = 800.f;

    /** 시작 시 Strength 자동 계산 */
    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    bool bAutoComputeStrength = true;

    /* ===== Runtime ===== */

    /** 자기장 안의 금속들 */
    UPROPERTY()
    TSet<UPrimitiveComponent*> OverlappingMetals;

    /* ===== Overlap ===== */

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
