#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Battery.generated.h"

class AWire;
class UBoxComponent;
class UStaticMeshComponent;
class UInputComponent;
class APlayerController;

UCLASS()
class MATERIAL_API ABATTERY : public AActor
{
    GENERATED_BODY()

public:
    ABATTERY();

    UFUNCTION(BlueprintCallable, Category = "Battery")
    void TogglePower();

    UFUNCTION(BlueprintCallable, Category = "Battery")
    void RefreshConnectedWires();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battery")
    bool bPowered = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battery|Electrical")
    float Voltage = 12.0f;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> BatteryMesh;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> InteractionBox;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> ConnectionOutlet;

    UPROPERTY()
    TArray<TObjectPtr<AWire>> ConnectedWires;

    UPROPERTY()
    TObjectPtr<APlayerController> CachedPlayerController;

    UPROPERTY()
    TObjectPtr<UInputComponent> BatteryInputComponent;

    FTimerHandle RefreshTimerHandle;
    bool bPlayerInRange = false;

    void SetupInputBinding();
    void RemoveInputBinding();
    void UpdateWiresPower();

    void OnHoldPressed();
    void OnHoldReleased();

    UFUNCTION()
    void OnInteractionBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnInteractionBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    UFUNCTION()
    void OnConnectionOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnConnectionEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};