#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoltageTester.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class AWire;
class AResistance;
class USoundBase;

UCLASS()
class MATERIAL_API AVoltageTester : public AActor
{
    GENERATED_BODY()

public:
    AVoltageTester();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComp;

    // 목표/현재 전압 표시 텍스트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UTextRenderComponent> StatusText;

    // ── 퍼즐 설정 ──
    // 측정할 전선 (에디터에서 지정). 이 전선 끝에 달린 저항의 전압강하를 측정.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tester")
    TObjectPtr<AWire> MeasureWire;

    // 이 값이 측정되면 정답 (예: 7.5V = 저항 양단 전압강하)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tester")
    float TargetVoltage = 7.5f;

    // 허용 오차 (예: ±0.3V)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tester")
    float Tolerance = 0.3f;

    // 정답 전압이 이 시간(초) 동안 유지되어야 문이 열림 (스치는 값 방지)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tester")
    float RequiredHoldTime = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tester")
    float RefreshInterval = 0.1f;

    // ── 문 설정 (OpenDoor 방식: 좌우 문이 서로 반대 방향으로 열림) ──
    // 왼쪽 문 (OpenDirection 방향으로 열림)
    UPROPERTY(EditAnywhere, Category = "Tester|Door")
    TObjectPtr<AActor> LeftDoorActor;

    // 오른쪽 문 (OpenDirection 반대 방향으로 열림)
    UPROPERTY(EditAnywhere, Category = "Tester|Door")
    TObjectPtr<AActor> RightDoorActor;

    UPROPERTY(EditAnywhere, Category = "Tester|Door")
    FVector OpenDirection = FVector(0.f, 1.f, 0.f);

    UPROPERTY(EditAnywhere, Category = "Tester|Door")
    float OpenDistance = 150.f;

    UPROPERTY(EditAnywhere, Category = "Tester|Door")
    float OpenSpeed = 200.f;

    // 문 열릴 때 재생할 효과음
    UPROPERTY(EditAnywhere, Category = "Tester|Sound")
    TObjectPtr<USoundBase> DoorOpenSound;

    // ── 디버그 ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tester|Debug")
    bool bDrawDebug = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tester|Debug")
    float MeasuredVoltage = 0.f;

    bool IsSolved() const { return bSolved; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    void RefreshMeasurement();
    void UpdateStatusText();

    // 이번 측정에서 찾은 저항 블럭 (MeasureWire 끝에 달린 것)
    UPROPERTY(Transient)
    TObjectPtr<AResistance> MeasuredResistor;

    FTimerHandle RefreshTimerHandle;

    float   HoldTimer = 0.f;   // 정답 유지 시간 누적
    bool    bSolved   = false; // 정답 확정 (문 열기 시작)
    bool    bOpened   = false; // 문 완전히 열림
    FVector LeftOpenLocation;
    FVector RightOpenLocation;
};
