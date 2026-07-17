#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "CircuitSwitchPlate.generated.h"

class AWire;
class USoundBase;

// ★ 회로에 낀 발판: 한 번 밟으면 끊겨 있던 전선 구간이 영구 연결됨
//    (UpstreamWire → DownstreamWires 를 ManualDownstreamWires로 이어줌)
UCLASS()
class MATERIAL_API ACircuitSwitchPlate : public AActor
{
    GENERATED_BODY()

public:
    ACircuitSwitchPlate();

    // ── 끊긴 구간의 앞쪽 전선 (전기가 들어오는 쪽) ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Circuit")
    TObjectPtr<AWire> UpstreamWire;

    // ── 발판이 눌리면 앞쪽 전선에 이어줄 다음 전선들 ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Circuit")
    TArray<TObjectPtr<AWire>> DownstreamWires;

    // ── 발판을 누를 수 있는 물체 태그 ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
    FName KeyTag = TEXT("PowerKey");

    // 태그 상관없이 아무 물체나 밟아도 작동할지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
    bool bAnyActorCanPress = false;

    // 이미 연결됐는지 (한 번 연결되면 다시 안 끊김)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Circuit")
    bool bConnected = false;

    // ── 사운드 ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    TObjectPtr<USoundBase> PedalSound;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> TriggerBox;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    void ConnectCircuit();
};
