// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage1_SecondPlatform.h"
#include "Stage1_FirstDoor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Transformation_actor.h"
#include "Engine/Engine.h"

AStage1_SecondPlatform::AStage1_SecondPlatform()
	: bCharacterOnPlatform(false)
	, bTransformActorOnPlatform(false)
	, bActivated(false)
{
	PrimaryActorTick.bCanEverTick = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	
	TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AStage1_SecondPlatform::OnOverlapBegin);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AStage1_SecondPlatform::OnOverlapEnd);

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	PlatformMesh->SetupAttachment(RootComponent);
}

void AStage1_SecondPlatform::BeginPlay()
{
	Super::BeginPlay();
}

void AStage1_SecondPlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bCharacterOnPlatform && !bTransformActorOnPlatform && !bActivated)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Yellow, TEXT("액터를 올려주세요"));
		}
	}
}

void AStage1_SecondPlatform::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor)
		return;

	if (OtherActor->IsA(ACharacter::StaticClass()))
	{
		bCharacterOnPlatform = true;
	}
	else if (OtherActor->IsA(ATransformation_actor::StaticClass()))
	{
		bTransformActorOnPlatform = true;

		if (!bActivated)
		{
			bActivated = true;

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("플랫폼 활성화! 문이 열립니다"));
			}

			if (DoorToOpen)
			{
				DoorToOpen->OpenDoor();
			}
		}
	}
}

void AStage1_SecondPlatform::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor)
		return;

	if (OtherActor->IsA(ACharacter::StaticClass()))
	{
		bCharacterOnPlatform = false;
	}
	else if (OtherActor->IsA(ATransformation_actor::StaticClass()))
	{
		bTransformActorOnPlatform = false;
	}
}