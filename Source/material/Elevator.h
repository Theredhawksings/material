// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Elevator.generated.h"

class ULevelSequence;
class ULevelSequencePlayer;
class ALevelSequenceActor;
class UBoxComponent;

UCLASS()
class MATERIAL_API AElevator : public AActor
{
	GENERATED_BODY()
	
public:	
	AElevator();

	// 기획자가 준 시퀀스 에셋(RootNode_TrackSetNode)을 여기 지정
	UPROPERTY(EditAnywhere, Category = "Elevator")
	TObjectPtr<ULevelSequence> DoorSequence;

	// 시퀀스 위치 보정용 앵커. 비워두면 보정 안 함.
	UPROPERTY(EditAnywhere, Category = "Elevator")
	TObjectPtr<AActor> TransformOrigin;

	UFUNCTION(BlueprintCallable, Category = "Elevator")
	void OpenDoor();

	UFUNCTION(BlueprintCallable, Category = "Elevator")
	void CloseDoor();

	UFUNCTION(BlueprintCallable, Category = "Elevator")
	void ToggleDoor();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Elevator")
	TObjectPtr<UBoxComponent> TriggerBox;

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> SequencePlayer;

	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> SequenceActor;

	bool bIsOpen = false;

	UFUNCTION()
	void OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};