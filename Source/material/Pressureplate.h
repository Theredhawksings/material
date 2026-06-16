#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PressurePlate.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class AMagnet;

// ★ 자석 하나의 설정을 묶은 구조체
USTRUCT(BlueprintType)
struct FMagnetSlot
{
    GENERATED_BODY()

    // 연결할 자석
    UPROPERTY(EditAnywhere, Category = "MagnetSlot")
    TObjectPtr<AMagnet> Magnet = nullptr;

    // ★ true = 처음에 내려가 있음, false = 처음에 올라가 있음
    UPROPERTY(EditAnywhere, Category = "MagnetSlot")
    bool bStartSunken = false;
};

UCLASS()
class MATERIAL_API APressurePlate : public AActor
{
    GENERATED_BODY()

public:
    APressurePlate();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY(VisibleAnywhere, Category = "PressurePlate")
    TObjectPtr<UStaticMeshComponent> PlateMesh;

    UPROPERTY(VisibleAnywhere, Category = "PressurePlate")
    TObjectPtr<UBoxComponent> DetectBox;

    // ★ 자석 슬롯 배열 - 에디터에서 자석 지정 + 초기 상태 설정
    UPROPERTY(EditAnywhere, Category = "PressurePlate")
    TArray<FMagnetSlot> MagnetSlots;

    // ★ 이동 거리
    UPROPERTY(EditAnywhere, Category = "PressurePlate")
    float MoveDistance = 300.f;

    // ★ 이동 속도
    UPROPERTY(EditAnywhere, Category = "PressurePlate")
    float MoveSpeed = 80.f;

    // 런타임 상태
    TArray<FVector> MagnetOriginalLocations;
    TArray<FVector> MagnetTargetLocations;
    TArray<bool>    MagnetMovedStates;

    TSet<AActor*> OverlappingPlayers;

    UFUNCTION()
    void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    void ToggleMagnets();
};