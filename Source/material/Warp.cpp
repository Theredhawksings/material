// Fill out your copyright notice in the Description page of Project Settings.

#include "Warp.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

AWarp::AWarp()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AWarp::OnOverlapBegin);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWarp::BeginPlay()
{
	Super::BeginPlay();
}

void AWarp::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AWarp::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ACharacter* Char = Cast<ACharacter>(OtherActor);
	if (!Char)
		return;

	// 같은 맵 안에서 위치만 순간이동 (로딩 없음)
	Char->SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);

	if (bApplyRotation)
	{
		if (AController* C = Char->GetController())
		{
			C->SetControlRotation(TargetRotation);
		}
	}
}