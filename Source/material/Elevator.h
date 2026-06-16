// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Elevator.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USceneComponent;
class USoundBase; // [추가]

UENUM()
enum class EElevatorState : uint8
{
    Idle,
    DoorOpening,
    Boarding,
    DoorClosing,
    Done,
    ArrivalOpening,
    Disabled // [추가] 영구 정지 상태
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

    // [수정] 엘리베이터 이동 시간 (기존 TeleportDelay) -> 여유 있게 3초로 변경
    UPROPERTY(EditAnywhere, Category = "Elevator|Timing")
    float TravelTime = 3.0f;

    // --- [추가] 사운드 ---
    UPROPERTY(EditAnywhere, Category = "Elevator|Sound")
    TObjectPtr<USoundBase> OpenSound;   // 문 열림 사운드

    UPROPERTY(EditAnywhere, Category = "Elevator|Sound")
    TObjectPtr<USoundBase> CloseSound;  // 문 닫힘 사운드

    // 문이 움직이기 시작한 뒤 사운드가 나올 때까지의 딜레이(초)
    UPROPERTY(EditAnywhere, Category = "Elevator|Sound")
    float SoundDelay = 0.3f;

    // 디버그 메시지 on/off
    UPROPERTY(EditAnywhere, Category = "Elevator|Debug")
    bool bDebug = true;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Door1;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> DoorL;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> DoorR;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> TriggerBox;

    UFUNCTION()
    void OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    void CloseDoors();
    void TeleportPlayer();
    void SetDoorYaw(float Yaw);

    // [추가] 사운드 재생 헬퍼 (타이머로 호출됨)
    void PlayOpenSound();
    void PlayCloseSound();

    // 화면 + 로그 출력 헬퍼
    void DebugMsg(const FString& Msg, const FColor& Color = FColor::Green);

    EElevatorState State = EElevatorState::Idle;
    float PhaseElapsed = 0.f;

    UPROPERTY(Transient)
    TObjectPtr<AActor> Passenger;

    FTimerHandle BoardTimer;
    FTimerHandle TeleportTimer;
    FTimerHandle SoundTimer; // [추가] 사운드 딜레이용
};