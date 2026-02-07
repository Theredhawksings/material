// Fill out your copyright notice in the Description page of Project Settings.

#include "MoveStage1_2.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AMoveStage1_2::AMoveStage1_2()
{
	PrimaryActorTick.bCanEverTick = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	
	TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AMoveStage1_2::OnOverlapBegin);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
}

void AMoveStage1_2::BeginPlay()
{
	Super::BeginPlay();
}

void AMoveStage1_2::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AMoveStage1_2::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor->IsA(ACharacter::StaticClass()))
	{
		GetWorld()->GetTimerManager().SetTimer(
			LevelLoadTimerHandle,
			this,
			&AMoveStage1_2::LoadNextLevel,
			LoadDelay,
			false
		);
	}
}

void AMoveStage1_2::LoadNextLevel()
{
	UGameplayStatics::OpenLevel(this, LevelToLoad);
}