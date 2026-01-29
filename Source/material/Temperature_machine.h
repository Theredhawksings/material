#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Temperature_machine.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class ATemperature;

UENUM(BlueprintType)
enum class EMachineType : uint8
{
    Heater UMETA(DisplayName = "Heater"),
    Cooler UMETA(DisplayName = "Cooler")
};

UCLASS()
class MATERIAL_API ATemperature_machine : public AActor
{
    GENERATED_BODY()
    
public: 
    ATemperature_machine();
    virtual void Tick(float DeltaTime) override;
    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* DetectionBox;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection Box")
    FVector BoxExtent = FVector(200.0f, 200.0f, 30.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection Box")
    FVector BoxRelativeLocation = FVector(0.0f, 0.0f, 10.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine")
    bool bIsActive = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Machine")
    EMachineType MachineType = EMachineType::Heater;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Conduction")
    float SurfaceTemperature = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Conduction")
    float ThermalConductivity = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Conduction")
    float ContactAreaM2 = 0.02f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Conduction")
    float ConductionThicknessM = 0.005f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Object Properties")
    float ObjectMassKg = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Object Properties")
    float SpecificHeatCapacity = 900.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Efficiency", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float EfficiencyFactor = 0.7f;

    UPROPERTY()
    TArray<ATemperature*> OverlappingTemperatureActors;

    UFUNCTION()
    void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    void ApplyConductionHeatTransfer(float DeltaTime);
    float CalculateHeatTransferRate(float ObjectTemperature) const;

public:
    UFUNCTION(BlueprintCallable, Category = "Machine")
    void SetMachineActive(bool bActive);

    UFUNCTION(BlueprintPure, Category = "Machine")
    bool IsMachineActive() const { return bIsActive; }

    UFUNCTION(BlueprintCallable, Category = "Machine")
    void ToggleMachine();
};