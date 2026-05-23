#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "MainStage1_Platform2.generated.h"

UCLASS()
class MATERIAL_API AMainStage1_Platform2 : public AActor
{
    GENERATED_BODY()

public:
    AMainStage1_Platform2();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* TriggerBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* PlatformMesh;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
    // 감지할 태그 (기본값 "Metal")
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
    FName RequiredTag = FName("Metal");

    // 문 액터
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
    AActor* LeftDoorActor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
    AActor* RightDoorActor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
    float OpenDistance = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
    float OpenSpeed = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
    FVector OpenDirection = FVector(0.0f, 1.0f, 0.0f);

    // 디버그
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bDebugDraw = true;

private:
    bool bActivated  = false;
    bool bIsOpening  = false;
    bool bIsOpen     = false;
    float CurrentTime = 0.0f;

    FVector LeftStartLocation;
    FVector LeftTargetLocation;
    FVector RightStartLocation;
    FVector RightTargetLocation;
};