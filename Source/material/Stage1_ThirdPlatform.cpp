// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage1_ThirdPlatform.h"
#include "Stage1_FirstDoor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Transformation_actor.h"
#include "Engine/Engine.h"

AStage1_ThirdPlatform::AStage1_ThirdPlatform()
	: bActivated(false)
	, TrackedActor(nullptr)
	, bWasMetal(false)
{
	PrimaryActorTick.bCanEverTick = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	
	TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AStage1_ThirdPlatform::OnOverlapBegin);

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	PlatformMesh->SetupAttachment(RootComponent);
}

void AStage1_ThirdPlatform::BeginPlay()
{
	Super::BeginPlay();

	FTimerHandle InitTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(
		InitTimerHandle,
		[this]()
		{
			TArray<AActor*> OverlappingActors;
			TriggerBox->GetOverlappingActors(OverlappingActors, ATransformation_actor::StaticClass());

			for (AActor* Actor : OverlappingActors)
			{
				if (ATransformation_actor* TransformActor = Cast<ATransformation_actor>(Actor))
				{
					TrackedActor = TransformActor;
					break;
				}
			}
		},
		0.1f,
		false
	);
}

void AStage1_ThirdPlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (TrackedActor && !bActivated)
	{
		bool bIsRubber = TrackedActor->ActorHasTag(FName("Rubber"));

		if (bIsRubber)
		{
			bActivated = true;

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("Rubber 감지! 문이 열립니다"));
			}

			if (DoorToOpen)
			{
				DoorToOpen->OpenDoor();
			}
		}
	}
}

void AStage1_ThirdPlatform::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || bActivated)
		return;

	if (ATransformation_actor* TransformActor = Cast<ATransformation_actor>(OtherActor))
	{
		TrackedActor = TransformActor;
	}
}