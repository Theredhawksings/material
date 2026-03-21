// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage3_IcePlatform.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Transformation_actor.h"
#include "Engine/Engine.h"

AStage3_IcePlatform::AStage3_IcePlatform()
	: TrackedActor(nullptr)
	, bActivated(false)
	, bIsOpening(false)
	, bIsOpen(false)
	, CurrentTime(0.0f)
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

	if (LeftDoorActor)
	{
		LeftStartLocation = LeftDoorActor->GetActorLocation();
		LeftTargetLocation = LeftStartLocation + FVector(0.0f, -OpenDistance, 0.0f);
	}

	if (RightDoorActor)
	{
		RightStartLocation = RightDoorActor->GetActorLocation();
		RightTargetLocation = RightStartLocation + FVector(0.0f, OpenDistance, 0.0f);
	}

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

		if (bConditionMet)
		{
			bActivated = true;
			bIsOpening = true;

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("조건 충족! 문이 열립니다"));
			}

			if (LeftDoorActor)
			{
				TArray<UStaticMeshComponent*> MeshComps;
				LeftDoorActor->GetComponents<UStaticMeshComponent>(MeshComps);
				for (UStaticMeshComponent* Mesh : MeshComps)
				{
					Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
			}

			if (RightDoorActor)
			{
				TArray<UStaticMeshComponent*> MeshComps;
				RightDoorActor->GetComponents<UStaticMeshComponent>(MeshComps);
				for (UStaticMeshComponent* Mesh : MeshComps)
				{
					Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
			}
		}
		else if (GEngine)
		{
			bool bIsIce = TrackedActor->ActorHasTag(FName("Ice"));
			FVector Scale = TrackedActor->GetActorScale3D();
			
			FString StatusMsg = FString::Printf(TEXT("Ice: %s | Scale: %.2f | Required: %.2f"), 
				bIsIce ? TEXT("Yes") : TEXT("No"),
				Scale.X,
				RequiredScale);
			GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Yellow, StatusMsg);
		}
	}

	if (bIsOpening && !bIsOpen)
	{
		CurrentTime += DeltaTime * OpenSpeed;
		
		if (CurrentTime >= 1.0f)
		{
			CurrentTime = 1.0f;
			bIsOpen = true;
			bIsOpening = false;
		}

		if (LeftDoorActor)
		{
			FVector NewLeftLocation = FMath::Lerp(LeftStartLocation, LeftTargetLocation, CurrentTime);
			LeftDoorActor->SetActorLocation(NewLeftLocation);
		}

		if (RightDoorActor)
		{
			FVector NewRightLocation = FMath::Lerp(RightStartLocation, RightTargetLocation, CurrentTime);
			RightDoorActor->SetActorLocation(NewRightLocation);
		}
	}
}

void AStage3_IcePlatform::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor)
		return;

	if (ATransformation_actor* TransformActor = Cast<ATransformation_actor>(OtherActor))
	{
		TrackedActor = TransformActor;
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