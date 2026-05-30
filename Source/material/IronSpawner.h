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

    // 외부(트리거)에서 스포너를 가동시킬 함수
    void ActivateSpawner();
    
    void DeactivateSpawner();

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
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IronSpawner|Components")
    USceneComponent* DefaultRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IronSpawner|Components")
    UStaticMeshComponent* SpawnerMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IronSpawner|Components")
    USceneComponent* SpawnLocationComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "IronSpawner|Components")
    UBoxComponent* DestructionZoneComponent;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronSpawner|Config")
    TSubclassOf<ATransformation_actor> IronClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronSpawner|Config")
    float SpawnInterval;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronSpawner|Config")
    float IronLifeTime;

private:
    bool bIsActive = false;

    UPROPERTY()
    TArray<FIronSpawnData> SpawnedIronList;

    FTimerHandle SpawnTimerHandle;
};