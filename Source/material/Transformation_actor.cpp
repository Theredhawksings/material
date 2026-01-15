// Fill out your copyright notice in the Description page of Project Settings.


#include "Transformation_actor.h"

// Sets default values
ATransformation_actor::ATransformation_actor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ATransformation_actor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATransformation_actor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

