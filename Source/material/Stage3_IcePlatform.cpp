// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage3_IcePlatform.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Transformation_actor.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

AStage3_IcePlatform::AStage3_IcePlatform()
	: TrackedActor(nullptr)
	, bActivated(false)
{
	PrimaryActorTick.bCanEverTick = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	
	TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AStage3_IcePlatform::OnOverlapBegin);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AStage3_IcePlatform::OnOverlapEnd);

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	PlatformMesh->SetupAttachment(RootComponent);
}

void AStage3_IcePlatform::BeginPlay()
{
	Super::BeginPlay();

	FTimerHandle InitTimer;
	GetWorld()->GetTimerManager().SetTimer(
		InitTimer,
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

void AStage3_IcePlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (TrackedActor && !bActivated)
	{
		bool bConditionMet = IsConditionMet();

		if (GEngine)
		{
			bool bIsIce = TrackedActor->ActorHasTag(FName("Ice"));
			FVector Scale = TrackedActor->GetActorScale3D();
			
			FString StatusMsg = FString::Printf(TEXT("Ice: %s | Scale: %.2f | Required: %.2f | %s"), 
				bIsIce ? TEXT("Yes") : TEXT("No"),
				Scale.X,
				RequiredScale,
				bConditionMet ? TEXT("PASS") : TEXT("FAIL"));
			GEngine->AddOnScreenDebugMessage(1, 0.0f, bConditionMet ? FColor::Green : FColor::Yellow, StatusMsg);
		}
	}
}

void AStage3_IcePlatform::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || bActivated)
		return;

	if (ATransformation_actor* TransformActor = Cast<ATransformation_actor>(OtherActor))
	{
		TrackedActor = TransformActor;
	}
	else if (OtherActor->IsA(ACharacter::StaticClass()))
	{
		if (TrackedActor && IsConditionMet())
		{
			bActivated = true;

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("조건 충족! Stage4로 이동"));
			}

			FTimerHandle MoveTimer;
			GetWorld()->GetTimerManager().SetTimer(
				MoveTimer,
				[this]()
				{
					UGameplayStatics::OpenLevel(this, NextStageName);
				},
				1.0f,
				false
			);
		}
		else
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("조건 불충족: Ice 상태 & Scale 0.7 필요"));
			}
		}
	}
}

void AStage3_IcePlatform::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor)
		return;

	if (OtherActor == TrackedActor)
	{
		TrackedActor = nullptr;
	}
}

bool AStage3_IcePlatform::IsConditionMet() const
{
	if (!TrackedActor)
		return false;

	bool bIsIce = TrackedActor->ActorHasTag(FName("Ice"));
	FVector Scale = TrackedActor->GetActorScale3D();
	
	float ScaleTolerance = 0.05f;
	bool bScaleCorrect = FMath::Abs(Scale.X - RequiredScale) <= ScaleTolerance;

	return bIsIce && bScaleCorrect;
}