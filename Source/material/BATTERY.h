#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Battery.generated.h"

// Forward declaration
class ABP_Wire;

UCLASS()
class MATERIAL_API ABATTERY : public AActor
{
    GENERATED_BODY()
    
public:    
    ABATTERY();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* BatteryMesh;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* InteractionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* ConnectionOutlet;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battery")
    bool bPowered;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battery")
    TArray<AActor*> ConnectedWires;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battery")
    bool bPlayerInRange;

    UFUNCTION(BlueprintCallable, Category = "Battery")
    void TogglePower();

    UFUNCTION(BlueprintCallable, Category = "Battery")
    void RefreshConnectedWires();

    UFUNCTION(BlueprintCallable, Category = "Battery")
    void UpdateWiresPower();

private:
    UFUNCTION()
    void OnInteractionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, 
                                      AActor* OtherActor, 
                                      UPrimitiveComponent* OtherComp, 
                                      int32 OtherBodyIndex, 
                                      bool bFromSweep, 
                                      const FHitResult& SweepResult);

    UFUNCTION()
    void OnInteractionBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, 
                                    AActor* OtherActor, 
                                    UPrimitiveComponent* OtherComp, 
                                    int32 OtherBodyIndex);

    void OnHoldPressed();
    void OnHoldReleased();
    
    void SetupInputBinding();
    void RemoveInputBinding();

    APlayerController* CachedPlayerController;
    
    FTimerHandle RefreshTimerHandle;
};