// Fill out your copyright notice in the Description page of Project Settings.


#include "Temperature_machine.h"

// Sets default values
ATemperature_machine::ATemperature_machine()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ATemperature_machine::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATemperature_machine::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

