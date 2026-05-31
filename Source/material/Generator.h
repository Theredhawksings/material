#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Generator.generated.h"

class UStaticMeshComponent;
class AWire;
class AMagnet;

UCLASS()
class MATERIAL_API AGenerator : public AActor
{
    GENERATED_BODY()

public:
    AGenerator();

    // ★ CoilGun이 직접 읽을 수 있도록 public으로
    UFUNCTION(BlueprintCallable, Category = "Generator")
    float GetCurrentEMF() const { return CurrentEMF; }

    UFUNCTION(BlueprintCallable, Category = "Generator")
    bool IsCurrentPositive() const { return bCurrentPositive; }

    UFUNCTION(BlueprintCallable, Category = "Generator")
    void ActivateGenerator() { bGeneratorActive = true; }

    UFUNCTION(BlueprintCallable, Category = "Generator")
    void DeactivateGenerator() { bGeneratorActive = false; }

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Generator")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, Category = "Generator")
    TObjectPtr<UStaticMeshComponent> GeneratorMesh;

    UPROPERTY(EditAnywhere, Category = "Generator|Magnet")
    float MagnetDetectRadius = 1500.f;

    UPROPERTY(EditAnywhere, Category = "Generator|Coil")
    float RotationSpeed = 180.f;

    UPROPERTY(EditAnywhere, Category = "Generator|EMF")
    int32 CoilWindings = 100;

    UPROPERTY(EditAnywhere, Category = "Generator|EMF")
    float CoilRadiusCM = 15.f;

    UPROPERTY(EditAnywhere, Category = "Generator|EMF")
    float MinEMFThreshold = 0.1f;

    UPROPERTY(EditAnywhere, Category = "Generator|Circuit")
    float WireDetectRadius = 200.f;

    UPROPERTY(EditAnywhere, Category = "Generator|Circuit")
    FVector WireDetectOffset = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, Category = "Generator|EMF")
    float CurrentEMF = 0.f;

    UPROPERTY(VisibleAnywhere, Category = "Generator|EMF")
    float RotationAngle = 0.f;

    UPROPERTY(VisibleAnywhere, Category = "Generator|EMF")
    bool bCurrentPositive = true;

    UPROPERTY(EditAnywhere, Category = "Generator|Debug")
    bool bDebugDraw = true;

    UPROPERTY()
    TObjectPtr<AMagnet> NorthMagnet;

    UPROPERTY()
    TObjectPtr<AMagnet> SouthMagnet;

    UPROPERTY()
    TArray<TObjectPtr<AWire>> ConnectedWires;

    void DetectMagnets();
    void UpdateEMF(float DeltaTime);
    void UpdateCircuit();

    UPROPERTY(EditAnywhere, Category = "Generator")
    bool bGeneratorActive = false;
};