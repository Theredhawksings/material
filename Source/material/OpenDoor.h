#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OpenDoor.generated.h"

class USceneComponent;
class UBoxComponent;

UCLASS()
class MATERIAL_API AOpenDoor : public AActor
{
    GENERATED_BODY()

public:
    AOpenDoor();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY(VisibleAnywhere, Category = "OpenDoor")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, Category = "OpenDoor")
    TObjectPtr<UBoxComponent> DetectBox;

    // 왼쪽 문 (+Y 방향으로 열림)
    UPROPERTY(EditAnywhere, Category = "OpenDoor")
    TObjectPtr<AActor> LeftDoorActor;

    // 오른쪽 문 (-Y 방향으로 열림)
    UPROPERTY(EditAnywhere, Category = "OpenDoor")
    TObjectPtr<AActor> RightDoorActor;

    UPROPERTY(EditAnywhere, Category = "OpenDoor|Open")
    float OpenDistance = 150.f;

    UPROPERTY(EditAnywhere, Category = "OpenDoor|Open")
    float OpenSpeed = 2.f;

    bool bOpened  = false;
    bool bOpening = false;

    FVector LeftClosedLocation;
    FVector LeftOpenLocation;
    FVector RightClosedLocation;
    FVector RightOpenLocation;

    TSet<AActor*> OverlappingPlayers;

    UFUNCTION()
    void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};