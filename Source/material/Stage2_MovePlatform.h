// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Stage2_MovePlatform.generated.h"

UCLASS()
class MATERIAL_API AStage2_MovePlatform : public AActor
{
	GENERATED_BODY()
	
public:	
	AStage2_MovePlatform();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* TriggerBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* PlatformMesh;

	UFUNCTION()
	void OnCharacterOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, 
		const FHitResult& SweepResult);

	void LoadNextLevel();

	FTimerHandle LevelLoadTimerHandle;

public:	
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	FName LevelToLoad = FName("Stage1-3");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	float LoadDelay = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight")
	class AStage2_WeightPlatform* WeightPlatform;
};