#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PressurePlate.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class AMagnet;
class USoundBase;

// ★ 발판이 담당하는 자석 (밟으면 토글됨)
USTRUCT(BlueprintType)
struct FMagnetSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "MagnetSlot")
    TObjectPtr<AMagnet> Magnet = nullptr;
};

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

    UPROPERTY(VisibleAnywhere, Category = "PressurePlate")
    TObjectPtr<UBoxComponent> DetectBox;

    // ★ 이 발판이 토글할 자석들
    UPROPERTY(EditAnywhere, Category = "PressurePlate")
    TArray<FMagnetSlot> MagnetSlots;

    TSet<AActor*> OverlappingPlayers;

    UFUNCTION()
    void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    void ToggleMagnets();

    UPROPERTY(EditAnywhere, Category = "PressurePlate|Sound")
    TObjectPtr<USoundBase> PedalSound;
};