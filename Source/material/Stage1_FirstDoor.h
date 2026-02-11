// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Stage1_FirstDoor.generated.h"

UCLASS()
class MATERIAL_API AStage1_FirstDoor : public AActor
{
	GENERATED_BODY()
	
public:	
	AStage1_FirstDoor();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* DoorMesh;

	FVector StartLocation;
	FVector TargetLocation;
	bool bIsOpening;
	bool bIsOpen;
	float CurrentTime;

public:	
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Door")
	void OpenDoor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float OpenHeight = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float OpenSpeed = 2.0f;

	bool IsOpen() const { return bIsOpen; }
};