#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbsorptionCube.generated.h"

UCLASS()
class MATERIAL_API AAbsorptionCube : public AActor
{
    GENERATED_BODY()

public:
    AAbsorptionCube();

protected:
    virtual void BeginPlay() override;

public:
    UPROPERTY(EditAnywhere, Category = "Absorption")
    float GaugeAmount = 20.f;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* MeshComp;

    UFUNCTION(BlueprintCallable)
    void ChargeAllGauges();
};