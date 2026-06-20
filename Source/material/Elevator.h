#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Elevator.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USceneComponent;
class USoundBase;
class UAudioComponent;

UENUM()
enum class EElevatorState : uint8
{
    Idle,
    DoorOpening,
    Boarding,
    DoorClosing,
    Done,
    ArrivalOpening,
    Disabled
};

UCLASS()
class MATERIAL_API AElevator : public AActor
{
    GENERATED_BODY()

public:
    AElevator();
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(EditAnywhere, Category = "Elevator|Move")
    FVector DestinationLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category = "Elevator|Door")
    float DoorClosedYaw = 0.f;

    UPROPERTY(EditAnywhere, Category = "Elevator|Door")
    float DoorOpenYaw = 90.f;

    UPROPERTY(EditAnywhere, Category = "Elevator|Door")
    float DoorMoveDuration = 1.5f;

    UPROPERTY(EditAnywhere, Category = "Elevator|Timing")
    float BoardingTime = 3.f;

    UPROPERTY(EditAnywhere, Category = "Elevator|Timing")
    float TravelTime = 5.0f;

    UPROPERTY(EditAnywhere, Category = "Elevator|Sound")
    TObjectPtr<USoundBase> OpenSound;

    UPROPERTY(EditAnywhere, Category = "Elevator|Sound")
    TObjectPtr<USoundBase> CloseSound;

    UPROPERTY(EditAnywhere, Category = "Elevator|Sound")
    TObjectPtr<USoundBase> TeleportSound;

    UPROPERTY(EditAnywhere, Category = "Elevator|Sound")
    float SoundDelay = 0.3f;

    UPROPERTY(EditAnywhere, Category = "Elevator|Debug")
    bool bDebug = true;

    // ★ 진동 세기 조절 (에디터에서 조절 가능)
    UPROPERTY(EditAnywhere, Category = "Elevator|Shake")
    float ShakeIntensity = 3.0f;   // 기본값 1.0 -> 3.0으로 상향

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Door1;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> DoorL;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> DoorR;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> TriggerBox;

    UPROPERTY()
    TObjectPtr<UAudioComponent> TeleportAudioComp;

    UFUNCTION()
    void OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    void CloseDoors();
    void TeleportPlayer();
    void SetDoorYaw(float Yaw);
    void PlayOpenSound();
    void PlayCloseSound();
    void DebugMsg(const FString& Msg, const FColor& Color = FColor::Green);

    EElevatorState State = EElevatorState::Idle;
    float PhaseElapsed = 0.f;

    UPROPERTY(Transient)
    TObjectPtr<AActor> Passenger;

    FTimerHandle BoardTimer;
    FTimerHandle TeleportTimer;
    FTimerHandle SoundTimer;

    // ★ 진동용
    FVector OriginalLocation = FVector::ZeroVector;
    float ShakeElapsed = 0.f;
};