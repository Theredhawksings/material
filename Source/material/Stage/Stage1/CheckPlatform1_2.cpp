// Fill out your copyright notice in the Description page of Project Settings.

#include "CheckPlatform1_2.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Transformation_actor.h"

ACheckPlatform1_2::ACheckPlatform1_2()
	: bHasTransformActor(false)
{
	PrimaryActorTick.bCanEverTick = true;

	DetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionBox"));
	RootComponent = DetectionBox;
	
	DetectionBox->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
	DetectionBox->SetCollisionProfileName(TEXT("Trigger"));
	DetectionBox->OnComponentBeginOverlap.AddDynamic(this, &ACheckPlatform1_2::OnObjectBeginOverlap);
	DetectionBox->OnComponentEndOverlap.AddDynamic(this, &ACheckPlatform1_2::OnObjectEndOverlap);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
}

void ACheckPlatform1_2::BeginPlay()
{
	Super::BeginPlay();
}

void ACheckPlatform1_2::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ACheckPlatform1_2::OnObjectBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor->IsA(ATransformation_actor::StaticClass()))
	{
		bHasTransformActor = true;
	}
}

void ACheckPlatform1_2::OnObjectEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor && OtherActor->IsA(ATransformation_actor::StaticClass()))
	{
		bHasTransformActor = false;
	}
}