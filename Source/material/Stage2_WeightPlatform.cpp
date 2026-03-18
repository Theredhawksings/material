// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage2_WeightPlatform.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Transformation_actor.h"
#include "Engine/Engine.h"

AStage2_WeightPlatform::AStage2_WeightPlatform()
{
	PrimaryActorTick.bCanEverTick = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	
	TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AStage2_WeightPlatform::OnOverlapBegin);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AStage2_WeightPlatform::OnOverlapEnd);

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	PlatformMesh->SetupAttachment(RootComponent);
}

void AStage2_WeightPlatform::BeginPlay()
{
	Super::BeginPlay();
}

void AStage2_WeightPlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	float CurrentWeight = CalculateTotalWeight();
	bool bInRange = IsWeightInRange();

	if (WeightTextActor)
	{
		UTextRenderComponent* TextComp = WeightTextActor->FindComponentByClass<UTextRenderComponent>();
		if (TextComp)
		{
			FString WeightMsg = FString::Printf(TEXT("Weight: %.1f"), CurrentWeight);	
			TextComp->SetText(FText::FromString(WeightMsg));
			TextComp->SetTextRenderColor(bInRange ? FColor::Green : FColor::White);
		}
	}
}

void AStage2_WeightPlatform::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this)
		return;

	if (OtherActor->IsA(ATransformation_actor::StaticClass()))
	{
		ActorsOnPlatform.AddUnique(OtherActor);
	}
}

void AStage2_WeightPlatform::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor)
		return;

	ActorsOnPlatform.Remove(OtherActor);
}

float AStage2_WeightPlatform::CalculateTotalWeight()
{
	float TotalWeight = 0.0f;

	for (AActor* Actor : ActorsOnPlatform)
	{
		if (!Actor)
			continue;

		TArray<UPrimitiveComponent*> PrimComps;
		Actor->GetComponents<UPrimitiveComponent>(PrimComps);

		for (UPrimitiveComponent* Prim : PrimComps)
		{
			if (Prim && Prim->IsSimulatingPhysics())
			{
				TotalWeight += Prim->GetMass();
			}
		}
	}

	return TotalWeight;
}

bool AStage2_WeightPlatform::IsWeightInRange() const
{
	float TotalWeight = 0.0f;

	for (AActor* Actor : ActorsOnPlatform)
	{
		if (!Actor)
			continue;

		TArray<UPrimitiveComponent*> PrimComps;
		Actor->GetComponents<UPrimitiveComponent>(PrimComps);

		for (UPrimitiveComponent* Prim : PrimComps)
		{
			if (Prim && Prim->IsSimulatingPhysics())
			{
				TotalWeight += Prim->GetMass();
			}
		}
	}

	return TotalWeight >= MinWeight && TotalWeight <= MaxWeight;
}