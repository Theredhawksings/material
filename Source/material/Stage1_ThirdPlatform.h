// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Stage1_ThirdPlatform.generated.h"

UCLASS()
class MATERIAL_API AStage1_ThirdPlatform : public AActor
{
	GENERATED_BODY()
	
public:	
	AStage1_ThirdPlatform();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* TriggerBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* PlatformMesh;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, 
		const FHitResult& SweepResult);

	bool bActivated;
	class ATransformation_actor* TrackedActor;
	bool bWasMetal;
	bool bIsOpening;
	bool bIsOpen;
	float CurrentTime;
	FVector LeftStartLocation;
	FVector LeftTargetLocation;
	FVector RightStartLocation;
	FVector RightTargetLocation;

public:	
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	AActor* LeftDoorActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	AActor* RightDoorActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float OpenDistance = -150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float OpenSpeed = 2.0f;
};