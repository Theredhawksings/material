// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage3_MovePlatform.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/Engine.h"

AStage3_MovePlatform::AStage3_MovePlatform()
{
	PrimaryActorTick.bCanEverTick = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	
	TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AStage3_MovePlatform::OnCharacterOverlap);

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	PlatformMesh->SetupAttachment(RootComponent);
}

void AStage3_MovePlatform::BeginPlay()
{
	Super::BeginPlay();
}

void AStage3_MovePlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AStage3_MovePlatform::OnCharacterOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor->IsA(ACharacter::StaticClass()))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("Stage4로 이동합니다"));
		}

		GetWorld()->GetTimerManager().SetTimer(
			LevelLoadTimerHandle,
			this,
			&AStage3_MovePlatform::LoadNextLevel,
			LoadDelay,
			false
		);
	}
}

void AStage3_MovePlatform::LoadNextLevel()
{
	UGameplayStatics::OpenLevel(this, LevelToLoad);
}