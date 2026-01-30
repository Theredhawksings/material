#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Wire.generated.h"

class USceneComponent;
class USplineComponent;
class USplineMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMesh;
class USphereComponent;

UCLASS()
class MATERIAL_API AWire : public AActor
{
    GENERATED_BODY()

public:
    AWire();

    UFUNCTION(BlueprintCallable, Category="Wire|Power")
    void SetPowered(bool bNewPowered);

    UFUNCTION(BlueprintPure, Category="Wire|Power")
    bool IsPowered() const { return bPowered; }

    UFUNCTION(BlueprintCallable, Category="Wire|Connection")
    void RefreshConnectedActors();

    UFUNCTION(BlueprintCallable, Category="Wire|Visual")
    void ApplyPower();

    UFUNCTION(BlueprintCallable, Category="Wire|Build")
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
    TObjectPtr<UMaterialInterface> SegmentMaterial;

    UPROPERTY(EditAnywhere, Category="Wire|Build")
    FVector2D SegmentScale = FVector2D(0.03f, 0.03f);

    // Wire.h 수정
	UPROPERTY(EditAnywhere, Category="Wire|Visual")
	TObjectPtr<UMaterialInterface> OffMaterial; // 여기에 꺼진 머터리얼 드래그

	UPROPERTY(EditAnywhere, Category="Wire|Visual")
	TObjectPtr<UMaterialInterface> OnMaterial;  // 여기에 켜진 머터리얼 드래그

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wire|Power", meta=(AllowPrivateAccess="true"))
    bool bPowered = false;

    UPROPERTY(EditAnywhere, Category="Wire|Connection")
    float OverlapRadius = 30.f;

    UPROPERTY(EditAnywhere, Category="Wire|Connection")
    TSubclassOf<AActor> ConnectableClass;

    UPROPERTY(EditAnywhere, Category="Wire|Connection")
    float RefreshInterval = 0.05f;

    UPROPERTY(EditAnywhere, Category="Wire|Connection")
    bool bCheckBothEnds = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wire|Connection")
    TArray<TObjectPtr<AActor>> ConnectedActors;
	
private:
    void ClearGeneratedMeshes();
    void GatherOverlapsAt(const FVector& WorldPos, TArray<AActor*>& OutActors) const;
    void PropagatePowerToConnected();
    void UpdateConnectionPoint();

private:
    UPROPERTY(Transient)
    TArray<TObjectPtr<USplineMeshComponent>> SegmentMeshes;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDArray;

    FTimerHandle RefreshTimerHandle;
};