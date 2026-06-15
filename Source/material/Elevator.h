// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Elevator.generated.h"

class UBoxComponent;

UCLASS()
class MATERIAL_API AElevator : public AActor
{
	GENERATED_BODY()

public:
	AElevator();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Elevator")
	TObjectPtr<UBoxComponent> TriggerBox;
};