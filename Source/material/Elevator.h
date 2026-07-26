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

    // 열릴 때 문이 도는 각도 (DoorL은 +값, DoorR은 -값 방향으로 회전)
    UPROPERTY(EditAnywhere, Category = "Elevator|Door")
    float DoorOpenYaw = 40.f;

    UPROPERTY(EditAnywhere, Category = "Elevator|Door")
    float DoorMoveDuration = 1.5f;

    // 문이 열린 뒤 실제 탑승이 확인될 때까지 최대로 기다리는 시간 (초과 시 아무도 안 태우고 문 닫음)
    UPROPERTY(EditAnywhere, Category = "Elevator|Timing")
    float BoardingTime = 3.f;

    // 탑승이 확인된 후 문을 닫기까지 여유 시간
    UPROPERTY(EditAnywhere, Category = "Elevator|Timing")
    float PostBoardCloseDelay = 1.5f;

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

    // 맵 로드 직후 이 시간(초) 동안은 트리거 오버랩 무시
    // (패키징 빌드에서 스폰 시 초기 오버랩 이벤트가 자동 발송되는 문제 방지)
    UPROPERTY(EditAnywhere, Category = "Elevator|Trigger")
    float TriggerGraceTime = 2.0f;

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

    // 엘리베이터 중앙(내부 탑승 공간)에 배치되는 전용 박스 - 실제 탑승 확인용
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> BoardingBox;

    UPROPERTY()
    TObjectPtr<UAudioComponent> TeleportAudioComp;

    // 문 앞 접근 감지 (TriggerBox) - 문 열기만 담당
    UFUNCTION()
    void OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    // 중앙 탑승 확인 (BoardingBox) - 실제로 안에 들어왔는지만 담당
    UFUNCTION()
    void OnBoardingBoxBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    // 탑승 확정 후 문 닫히기 전에 BoardingBox를 벗어나면 탑승 취소
    UFUNCTION()
    void OnBoardingBoxEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    bool IsTriggerGraceActive() const;

    // 실제 탑승이 확인된 순간 호출 - PostBoardCloseDelay 뒤 문 닫기 예약
    void OnBoardingConfirmed();
    // 최대 대기시간 동안 아무도 안 타면 호출 - 문 닫고 대기 상태로 복귀 (이동 없음)
    void OnBoardingTimeout();

    void CloseDoors();
    virtual void TeleportPlayer();
    void SetDoorYaw(float Yaw);
    void PlayOpenSound();
    void PlayCloseSound();
    void DebugMsg(const FString& Msg, const FColor& Color = FColor::Green);

    EElevatorState State = EElevatorState::Idle;
    float PhaseElapsed = 0.f;
    float BeginPlayTimeSeconds = 0.f;

    UPROPERTY(Transient)
    TObjectPtr<AActor> Passenger;

    // Boarding 상태에서 실제 탑승이 확인됐는지 (트리거박스 접촉 한 번이 아니라 문 닫기 직전까지 유지되어야 함)
    bool bBoardConfirmed = false;

    FTimerHandle BoardTimer;
    FTimerHandle BoardConfirmTimer;
    FTimerHandle TeleportTimer;
    FTimerHandle SoundTimer;

    // ★ 진동용
    FVector OriginalLocation = FVector::ZeroVector;
    float ShakeElapsed = 0.f;

    // 에디터에서 배치한 문의 원래 회전 (닫힘 기준값 - 게임 시작 시 저장)
    FQuat DoorLBaseQuat = FQuat::Identity;
    FQuat DoorRBaseQuat = FQuat::Identity;
};