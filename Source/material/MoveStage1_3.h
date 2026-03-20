// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "MoveStage1_3.generated.h"

UCLASS()
class MATERIAL_API AMoveStage1_3 : public AActor
{
	GENERATED_BODY()
	
public:	
	AMoveStage1_3();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* TriggerBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

	UFUNCTION()
	void OnCharacterOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, 
		const FHitResult& SweepResult);

	void LoadNextLevel();

	FTimerHandle LevelLoadTimerHandle;

public:	
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	FName LevelToLoad = FName("Stage3");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	float LoadDelay = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CheckPlatform")
	class ACheckPlatform1_2* CheckPlatform;
};