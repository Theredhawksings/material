#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AirConditioner.generated.h"

UCLASS()
class MATERIAL_API AAirConditioner : public AActor
{
    GENERATED_BODY()

public:
    AAirConditioner();

    // TransformPowerTrigger가 호출할 함수
    UFUNCTION(BlueprintCallable, Category = "AirConditioner")
    void ActivateAircon();

    UFUNCTION(BlueprintCallable, Category = "AirConditioner")
    void DeactivateAircon();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AirConditioner")
    bool bIsRunning = false;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    // ── 에어컨 메시 (C++에서 에셋 지정) ──
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComp;

    // ── 감지 박스 (에디터에서 위치·크기 자유 조정) ──
    UPROPERTY(EditAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> DetectionBox;
};