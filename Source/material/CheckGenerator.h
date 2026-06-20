// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheckGenerator.generated.h"

class AGenerator; 
class USoundBase;

UCLASS()
class MATERIAL_API ACheckGenerator : public AActor
{
    GENERATED_BODY()
    
public: 
    ACheckGenerator();

protected:
    virtual void BeginPlay() override;

public: 
    virtual void Tick(float DeltaTime) override;

    // 작동 유무를 확인할 발전기들을 할당할 배열
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
    TArray<AGenerator*> AssignedGenerators;

    // 문 열림 제어 변수들
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
    USoundBase* DoorOpenSound;

private:
    bool bActivated = false;
    bool bIsOpening = false;
    bool bIsOpen = false;
    float CurrentTime = 0.0f;
    
    FVector LeftStartLocation;
    FVector LeftTargetLocation;
    FVector RightStartLocation;
    FVector RightTargetLocation;
};