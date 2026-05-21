#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
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
    // 루트 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IronSpawner|Components")
    USceneComponent* DefaultRoot;

    // ★ 스포너 본체 메시 (에디터에서 메시/머터리얼 지정 가능)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IronSpawner|Components")
    UStaticMeshComponent* SpawnerMesh;

    // 소환 위치 (SpawnerMesh에 부착)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IronSpawner|Components")
    USceneComponent* SpawnLocationComponent;

    // 삭제 구역 박스
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