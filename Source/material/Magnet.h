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

    float TimeSinceLastRefresh = 0.f;
    float BaseStrength = 0.f;

    void UpdateElectroBoost();
    void ApplyInducedMagnetism();
    float CalculateInducedStrength(float DistanceToMagnet, float BaseMagnetStrength) const;
    void CheckDemagnetize();

    static constexpr float MaxForceClamp = 6e7f;
    static constexpr float MaxInducedForceClamp = 3e7f;
    static constexpr float GravityAccel = 980.f;
};