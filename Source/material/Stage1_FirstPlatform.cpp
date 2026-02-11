// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage1_FirstPlatform.h"
#include "Stage1_FirstDoor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/Engine.h"

AStage1_FirstPlatform::AStage1_FirstPlatform()
	: bActivated(false)
{
	PrimaryActorTick.bCanEverTick = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	
	TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AStage1_FirstPlatform::OnCharacterOverlap);

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	PlatformMesh->SetupAttachment(RootComponent);
}

void AStage1_FirstPlatform::BeginPlay()
{
	Super::BeginPlay();
}

void AStage1_FirstPlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AStage1_FirstPlatform::OnCharacterOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bActivated)
		return;

	if (OtherActor && OtherActor->IsA(ACharacter::StaticClass()))
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