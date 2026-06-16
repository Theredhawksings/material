#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ElectricDoor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class AWire;

UCLASS()
class MATERIAL_API AElectricDoor : public AActor
{
    GENERATED_BODY()

public:
    AElectricDoor();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    // 이 액터 자체는 보이지 않는 컨트롤러 역할
    UPROPERTY(VisibleAnywhere, Category = "Door")
    TObjectPtr<USceneComponent> Root;

    // 전선 끝을 갖다 대는 충돌박스
    UPROPERTY(VisibleAnywhere, Category = "Door")
    TObjectPtr<UBoxComponent> WireContactBox;

    // ★ 에디터에서 흰색 큐브 액터를 직접 지정
    UPROPERTY(EditAnywhere, Category = "Door")
    TObjectPtr<AActor> DoorActor;

    // ★ 열리는 방향과 거리 (에디터에서 직접 설정)
    UPROPERTY(EditAnywhere, Category = "Door|Open")
    FVector OpenDirection = FVector(1.f, 0.f, 0.f);

    UPROPERTY(EditAnywhere, Category = "Door|Open")
    float OpenDistance = 300.f;

    // ★ 열리는 속도
    UPROPERTY(EditAnywhere, Category = "Door|Open")
    float OpenSpeed = 200.f;

    // 상태
    bool bOpened  = false;
    bool bOpening = false;

    FVector ClosedLocation;
    FVector OpenLocation;

    void CheckWirePower();
    void OpenDoor();
};