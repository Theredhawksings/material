#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Battery.generated.h"

class AWire;
class UBoxComponent;
class UStaticMeshComponent;
class UInputComponent;
class APlayerController;
class USoundBase;  
class UAudioComponent; 

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
    float Voltage = 48.0f;

    // 에디터/게임에서 디버그 셰이프(InteractionBox, ConnectionOutlet) 보이기 토글
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battery|Debug")
    bool bShowDebugShapes = true;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> BatteryMesh;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> InteractionBox;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> ConnectionOutlet;

    UPROPERTY()
    TObjectPtr<AWire> ConnectedWire;

    UPROPERTY()
    TObjectPtr<APlayerController> CachedPlayerController;

    UPROPERTY()
    TObjectPtr<UInputComponent> BatteryInputComponent;

    FTimerHandle RefreshTimerHandle;
    bool bPlayerInRange = false;

    void SetupInputBinding();
    void RemoveInputBinding();
    void UpdateWiresPower();
    void ApplyDebugVisibility();

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

    UPROPERTY(EditAnywhere, Category = "Battery|Sound")
    TObjectPtr<USoundBase> SparkSound;

    UPROPERTY()
    TObjectPtr<UAudioComponent> SparkAudioComp;

    UPROPERTY(EditAnywhere, Category = "Battery|Electrical")
    TObjectPtr<AWire> OutputWire; // +단자: 배터리가 전원을 내보내는 출발 전선

    UPROPERTY(EditAnywhere, Category = "Battery|Electrical")
    TObjectPtr<AWire> ReturnWire; // −단자: 회로가 배터리로 돌아오는 귀환 전선
};
\