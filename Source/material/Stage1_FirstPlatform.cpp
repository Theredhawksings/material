// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage1_FirstPlatform.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/Engine.h"

AStage1_FirstPlatform::AStage1_FirstPlatform()
	: bActivated(false)
	, bIsOpening(false)
	, bIsOpen(false)
	, CurrentTime(0.0f)
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
}

void AStage1_FirstPlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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

void AStage1_FirstPlatform::OnCharacterOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bActivated)
		return;

	if (OtherActor && OtherActor->IsA(ACharacter::StaticClass()))
	{
		bActivated = true;
		bIsOpening = true;

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("플랫폼 활성화! 문이 열립니다"));
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
}