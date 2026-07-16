#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "AirConditioner.h"
#include "WindModeToggleTrigger.generated.h"

class USoundBase;

// ★ 새 발판: 밟히면 지정된 에어컨(히터)들의 바람 모드를 전부 반전
//    (뜨거운/차가운/뜨거운 → 차가운/뜨거운/차가운)
UCLASS()
class MATERIAL_API AWindModeToggleTrigger : public AActor
{
    GENERATED_BODY()

public:
    AWindModeToggleTrigger();

    // ── 모드를 반전시킬 에어컨들 (에디터에서 히터 3개 지정) ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Targets")
    TArray<TObjectPtr<AAirConditioner>> TargetAircons;

    // ── 발판을 누를 수 있는 물체 태그 ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Targets")
    FName KeyTag = TEXT("PowerKey");

    // 태그 상관없이 아무 물체나 밟아도 작동할지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Targets")
    bool bAnyActorCanPress = false;

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
};
