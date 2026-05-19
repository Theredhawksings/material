// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Transformation_actor.h"
#include "IronSpawner.generated.h"

USTRUCT(BlueprintType)
struct FIronSpawnData
{
    GENERATED_BODY()

    UPROPERTY()
    ATransformation_actor* IronActor = nullptr;

    UPROPERTY()
    float TimeInZone = 0.0f;

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

    UFUNCTION(BlueprintCallable, Category = "IronSpawner")
    void OnIronConsumed(AActor* ConsumedIron);

protected:
    void SpawnIron();
    void CheckIronLifeTime(float DeltaTime);

    UFUNCTION()
    void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
                            UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
                            bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
                          UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

protected:
    // 루트 컴포넌트용 (스폰러 자체의 기준점)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IronSpawner|Components")
    USceneComponent* DefaultRoot;

    // 1. 소환하는 곳 (이제 에디터에서 개별 선택 후 이동 가능)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IronSpawner|Components")
    USceneComponent* SpawnLocationComponent;

    // 2. 삭제되는 곳 구역 박스 (이제 에디터에서 개별 선택 후 이동 및 크기 조절 가능)
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