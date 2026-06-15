// Fill out your copyright notice in the Description page of Project Settings.

#include "Elevator.h"
#include "Components/BoxComponent.h"

AElevator::AElevator()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 100.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AElevator::BeginPlay()
{
	Super::BeginPlay();
}