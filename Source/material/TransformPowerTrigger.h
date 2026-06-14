#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "AirConditioner.h"
#include "TransformPowerTrigger.generated.h"

UCLASS()
class MATERIAL_API ATransformPowerTrigger : public AActor
{
    GENERATED_BODY()

public:
    ATransformPowerTrigger();

    // ── 에어컨 참조 (에디터에서 지정) ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Targets")
    TObjectPtr<AAirConditioner> TargetAircon;

    // ── KeyTag 그대로 유지 ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Targets")
    FName KeyTag = TEXT("PowerKey");

    // ── 문짝 참조 (에디터에서 좌/우 문 메시 액터 지정) ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
    TObjectPtr<AActor> LeftDoorActor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
    TObjectPtr<AActor> RightDoorActor;

    // 문 벌어지는 거리 (왼쪽은 -Y, 오른쪽은 +Y 방향으로 이 값만큼)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
    float OpenDistance = 150.0f;

    // 문 열림 속도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
    float OpenSpeed = 2.0f;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> TriggerBox;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    // ── 문 제어 ──
    void OpenDoor();

private:
    // 문 Lerp용 상태
    bool bIsOpening = false;
    bool bIsOpen = false;
    float CurrentTime = 0.f;

    FVector LeftStartLocation;
    FVector LeftTargetLocation;
    FVector RightStartLocation;
    FVector RightTargetLocation;
};