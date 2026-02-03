#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Battery.generated.h"

class AWire; // AWire로 클래스명 통일 (스크린샷 기준)

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

    UFUNCTION()
    void OnConnectionOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnConnectionEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    UFUNCTION()
    void OnInteractionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnInteractionBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    void OnHoldPressed();
    void OnHoldReleased();
    
    void SetupInputBinding();
    void RemoveInputBinding();

    UPROPERTY()
    APlayerController* CachedPlayerController;

    // [추가] 중복 바인딩 방지를 위한 전용 입력 컴포넌트
    UPROPERTY()
    UInputComponent* BatteryInputComponent;
    
    FTimerHandle RefreshTimerHandle;
};