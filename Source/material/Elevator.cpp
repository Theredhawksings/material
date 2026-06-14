// Fill out your copyright notice in the Description page of Project Settings.

#include "Elevator.h"
#include "Components/BoxComponent.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "MovieSceneSequencePlaybackSettings.h"

AElevator::AElevator()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 100.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AElevator::BeginPlay()
{
	Super::BeginPlay();

	if (DoorSequence)
	{
		FMovieSceneSequencePlaybackSettings Settings;
		Settings.bPauseAtEnd = true; // 끝에서 멈춰서 열린 상태 유지

		SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(
			GetWorld(), DoorSequence, Settings, SequenceActor);

		// --- 위치 보정 (선택) ---
		// 컴파일 에러 나면 이 블록 통째로 주석 처리하고
		// 에디터에서 LevelSequenceActor의 Override Instance Data로 설정하세요.
		if (SequenceActor && TransformOrigin)
		{
			SequenceActor->bOverrideInstanceData = true;
			SequenceActor->DefaultInstanceData.TransformOriginActor = TransformOrigin;
		}
		// ------------------------
	}

	if (TriggerBox)
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AElevator::OnTriggerBegin);
	}
}

void AElevator::OpenDoor()
{
	if (SequencePlayer && !bIsOpen)
	{
		SequencePlayer->Play();          // 정방향 = 열기
		bIsOpen = true;
	}
}

void AElevator::CloseDoor()
{
	if (SequencePlayer && bIsOpen)
	{
		SequencePlayer->PlayReverse();   // 역방향 = 닫기 (별도 애니 불필요)
		bIsOpen = false;
	}
}

void AElevator::ToggleDoor()
{
	bIsOpen ? CloseDoor() : OpenDoor();
}

void AElevator::OnTriggerBegin(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (OtherActor && OtherActor != this)
	{
		OpenDoor();
	}
}