// Fill out your copyright notice in the Description page of Project Settings.

#include "MoveStage1_3.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "CheckPlatform1_2.h"
#include "Engine/Engine.h"

AMoveStage1_3::AMoveStage1_3()
{
	PrimaryActorTick.bCanEverTick = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	
	TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AMoveStage1_3::OnCharacterOverlap);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
}

void AMoveStage1_3::BeginPlay()
{
	Super::BeginPlay();
}

void AMoveStage1_3::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AMoveStage1_3::OnCharacterOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor->IsA(ACharacter::StaticClass()))
	{
		if (CheckPlatform && CheckPlatform->HasTransformActor())
		{
			GetWorld()->GetTimerManager().SetTimer(
				LevelLoadTimerHandle,
				this,
				&AMoveStage1_3::LoadNextLevel,
				LoadDelay,
				false
			);
		}
		else
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("액터를 올리세요"));
			}
		}
	}
}

void AMoveStage1_3::LoadNextLevel()
{
	UGameplayStatics::OpenLevel(this, LevelToLoad);
}