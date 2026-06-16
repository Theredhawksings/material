#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PressurePlate.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class AMagnet;

UCLASS()
class MATERIAL_API APressurePlate : public AActor
{
    GENERATED_BODY()

public:
    APressurePlate();

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category = "PressurePlate")
    TObjectPtr<UStaticMeshComponent> PlateMesh;

    // 플레이어 감지 콜리전
    UPROPERTY(VisibleAnywhere, Category = "PressurePlate")
    TObjectPtr<UBoxComponent> DetectBox;

    // 에디터에서 연결할 자석 (1:1)
    UPROPERTY(EditAnywhere, Category = "PressurePlate")
    TObjectPtr<AMagnet> LinkedMagnet;

    // 현재 발판 위에 있는 액터 수 (여러 명 올라가도 안전하게 처리)
    int32 OverlapCount = 0;

    UFUNCTION()
    void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};