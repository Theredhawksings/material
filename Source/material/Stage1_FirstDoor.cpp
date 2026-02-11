// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage1_FirstDoor.h"

AStage1_FirstDoor::AStage1_FirstDoor()
	: bIsOpening(false)
	, bIsOpen(false)
	, CurrentTime(0.0f)
{
	PrimaryActorTick.bCanEverTick = true;

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	RootComponent = DoorMesh;
}

void AStage1_FirstDoor::BeginPlay()
{
	Super::BeginPlay();
	
	StartLocation = GetActorLocation();
	TargetLocation = StartLocation + FVector(0.0f, 0.0f, OpenHeight);
}

void AStage1_FirstDoor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsOpening && !bIsOpen)
	{
		CurrentTime += DeltaTime * OpenSpeed;
		
		if (CurrentTime >= 1.0f)
		{
			CurrentTime = 1.0f;
			bIsOpen = true;
			bIsOpening = false;
		}

		FVector NewLocation = FMath::Lerp(StartLocation, TargetLocation, CurrentTime);
		SetActorLocation(NewLocation);
	}
}

void AStage1_FirstDoor::OpenDoor()
{
	if (!bIsOpen && !bIsOpening)
	{
		bIsOpening = true;
		CurrentTime = 0.0f;
		
		if (DoorMesh)
		{
			DoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}