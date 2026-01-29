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

    UPROPERTY(VisibleAnywhere, Category="Magnet")
    UStaticMeshComponent* MagnetMesh;

    UPROPERTY(VisibleAnywhere, Category="Magnet")
    USphereComponent* MagnetRange;

    UPROPERTY(EditAnywhere, Category="Magnet")
    FName MetalTag = "Metal";

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float Strength;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float ReferenceDistance = 80.f;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float MaxLiftMass = 70.f;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float MinDistance = 10.f;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float MaxDistance = 600.f;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    bool bAutoComputeStrength = true;

    UPROPERTY(EditAnywhere, Category="Magnet|Physics")
    float ForceMultiplier = 100.0f;  // 50 -> 10

    UPROPERTY()
    TSet<UPrimitiveComponent*> OverlappingMetals;

    UPROPERTY(EditAnywhere, Category = "Magnet")
    float MagneticDecayExponent = 1.3f;  // 1.0 -> 1.3

    UPROPERTY(EditAnywhere, Category = "Magnet")
    float VelocityDampingFactor = 0.15f;  // 0.05 -> 0.15

    UPROPERTY(EditAnywhere, Category = "Magnet")
    float MaxAttractVelocity = 2000.f;  // 5000 -> 2000

    UPROPERTY(EditAnywhere, Category = "Magnet")
    bool bUseTorque = true;

    UPROPERTY(EditAnywhere, Category = "Magnet")
    bool bApplyInitialImpulse = false;  // true -> false

    UPROPERTY(EditAnywhere, Category = "Magnet")
    float InitialImpulseStrength = 200.f;

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bDebugDraw = true;

    UPROPERTY(EditAnywhere, Category = "Magnet|Advanced")
    float RefreshInterval = 0.1f;

    float TimeSinceLastRefresh = 0.f;

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