// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h" // 박스 컴포넌트 추가
#include "Transformation_actor.h"
#include "IronSpawner.generated.h"

USTRUCT(BlueprintType)
struct FIronSpawnData
{
    GENERATED_BODY()

    UPROPERTY()
    ATransformation_actor* IronActor = nullptr;

    // 바닥 구역(Trigger) 내부에 머무른 누적 시간
    UPROPERTY()
    float TimeInZone = 0.0f;

    // 현재 바닥 구역 안에 들어와 있는지 여부
    UPROPERTY()
    bool bIsInZone = false;
};

UCLASS()
class MATERIAL_API AIronSpawner : public AActor
{
    GENERATED_BODY()
    
public: 
    AIronSpawner();

protected:
    virtual void BeginPlay() override;

public: 
    virtual void Tick(float DeltaTime) override;

    // 코일건 흡수용 함수
    UFUNCTION(BlueprintCallable, Category = "IronSpawner")
    void OnIronConsumed(AActor* ConsumedIron);

protected:
    void SpawnIron();
    void CheckIronLifeTime(float DeltaTime);

    // ★ 충돌 구역 진입/퇴출 감지용 이벤트 함수
    UFUNCTION()
    void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
                            UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
                            bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
                          UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

protected:
    // 어디서 스폰할지 시각적으로 배치할 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IronSpawner|Components")
    USceneComponent* SpawnLocationComponent;

    // ★ 바닥 파괴 영역을 담당할 박스 콜리전 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IronSpawner|Components")
    UBoxComponent* DestructionZoneComponent;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronSpawner|Config")
    TSubclassOf<ATransformation_actor> IronClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronSpawner|Config")
    float SpawnInterval;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronSpawner|Config")
    float IronLifeTime;

private:
    UPROPERTY()
    TArray<FIronSpawnData> SpawnedIronList;

    FTimerHandle SpawnTimerHandle;
};