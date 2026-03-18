// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage2_MovePlatform.h"
#include "Stage2_WeightPlatform.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/Engine.h"

AStage2_MovePlatform::AStage2_MovePlatform()
{
	PrimaryActorTick.bCanEverTick = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	
	TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AStage2_MovePlatform::OnCharacterOverlap);

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	PlatformMesh->SetupAttachment(RootComponent);
}

void AStage2_MovePlatform::BeginPlay()
{
	Super::BeginPlay();
}

void AStage2_MovePlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AStage2_MovePlatform::OnCharacterOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor->IsA(ACharacter::StaticClass()))
	{
		if (WeightPlatform && WeightPlatform->IsWeightInRange())
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("무게 조건 충족! 이동합니다"));
			}

			GetWorld()->GetTimerManager().SetTimer(
				LevelLoadTimerHandle,
				this,
				&AStage2_MovePlatform::LoadNextLevel,
				LoadDelay,
				false
			);
		}
		else
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("무게가 범위를 벗어났습니다!"));
			}
		}
	}
}

void AStage2_MovePlatform::LoadNextLevel()
{
	UGameplayStatics::OpenLevel(this, LevelToLoad);
}