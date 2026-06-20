// EndingElevator.cpp
// Fill out your copyright notice in the Description page of Project Settings.

#include "EndingElevator.h"
#include "Elevator.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"

AEndingElevator::AEndingElevator()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AEndingElevator::BeginPlay()
{
	Super::BeginPlay();

	// 기존 엘리베이터의 TriggerBox에 엔딩용 오버랩 이벤트 추가 바인딩
	if (TriggerBox)
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AEndingElevator::OnEndingTriggerBegin);
	}
}

void AEndingElevator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bEndingStarted && bPlayerBoarded)
	{
		BoardElapsed += DeltaTime;

		if (BoardElapsed >= EndingStartDelay)
		{
			bEndingStarted = true;
			StartEnding();
		}
	}
}

void AEndingElevator::OnEndingTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (bPlayerBoarded) return;

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (OtherActor && OtherActor == PlayerPawn)
	{
		bPlayerBoarded = true;
	}
}

void AEndingElevator::StartEnding()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	// ① 브금 끄기
	if (TeleportAudioComp)
	{
		TeleportAudioComp->Stop();
	}

	// ② 카메라 서서히 암전 (먼저 천천히 깔림)
	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(
			0.f, 1.f,
			FadeDuration,
			FLinearColor::Black,
			false,
			true
		);
	}

	// ③ 입력 막기
	PC->SetInputMode(FInputModeUIOnly());

	// ④ TextDelay 후에 위젯 띄우기 (위젯이 뜰 때 자체 FadeIn 애니메이션 재생)
	FTimerHandle TextTimer;
	GetWorldTimerManager().SetTimer(TextTimer, this,
		&AEndingElevator::ShowEndingText, TextDelay, false);

	// ⑤ 다 본 후 메인 메뉴로 이동
	FTimerHandle EndTimer;
	GetWorldTimerManager().SetTimer(EndTimer, [this]()
	{
		UGameplayStatics::OpenLevel(this, MainMenuLevel);
	}, FadeDuration + TextDelay + AfterFadeDelay, false);
}

void AEndingElevator::ShowEndingText()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !EndingWidgetClass) return;

	EndingWidget = CreateWidget<UUserWidget>(PC, EndingWidgetClass);
	if (EndingWidget)
	{
		EndingWidget->AddToViewport();
	}
}