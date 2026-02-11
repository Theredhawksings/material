// Wire.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Wire.generated.h"

class USceneComponent;
class USplineComponent;
class USplineMeshComponent;
class UMaterialInterface;
class USphereComponent;
class UStaticMesh;
class ATransformation_actor;

UCLASS()
class MATERIAL_API AWire : public AActor
{
    GENERATED_BODY()

public:
    AWire();

    void SetPowered(bool bNewPowered);
    void SetPoweredByMetal(bool bNewPoweredByMetal);

    bool IsPowered() const { return bPoweredFinal; }
    bool IsSourcePowered() const { return bPoweredBySource; }

    void RefreshConnectedActors();
    void ApplyPower();
    void RebuildSplineMeshes();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    UPROPERTY(VisibleAnywhere, Category="Wire|Components")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, Category="Wire|Components")
    TObjectPtr<USplineComponent> Spline;

    UPROPERTY(VisibleAnywhere, Category="Wire|Components")
    TObjectPtr<USphereComponent> ConnectionSphere;

    UPROPERTY(EditAnywhere, Category="Wire|Build")
    TObjectPtr<UStaticMesh> SegmentMesh;

    UPROPERTY(EditAnywhere, Category="Wire|Build")
    FVector2D SegmentScale = FVector2D(0.03f, 0.03f);

    UPROPERTY(EditAnywhere, Category="Wire|Visual")
    TObjectPtr<UMaterialInterface> OffMaterial;

    UPROPERTY(EditAnywhere, Category="Wire|Visual")
    TObjectPtr<UMaterialInterface> OnMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wire|Power")
    bool bPoweredFinal = false;

    UPROPERTY(EditAnywhere, Category="Wire|Connection")
    float OverlapRadius = 30.f;

    UPROPERTY(EditAnywhere, Category="Wire|Connection")
    float RefreshInterval = 0.10f;

    UPROPERTY(EditAnywhere, Category="Wire|Debug")
    bool bDebugWire = true;

    UPROPERTY(EditAnywhere, Category="Wire|Debug")
    bool bDebugOnScreen = true;

private:
    void ClearGeneratedMeshes();
    void UpdateConnectionPoint();
    void UpdateFinalPower();
    void PropagatePowerToConnected();

private:
    UPROPERTY()
    TArray<TObjectPtr<USplineMeshComponent>> SegmentMeshes;

    UPROPERTY()
    TArray<TObjectPtr<AActor>> ConnectedActors;

    FTimerHandle RefreshTimerHandle;

    bool bPoweredBySource = false;
    bool bPoweredByMetal = false;

    float LastDebugTime = -1000.f;
};