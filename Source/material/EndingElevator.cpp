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
		TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AEndingElevator::OnEndingTriggerEnd);
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
			// ★ 최종 확인: 플레이어가 실제로 트리거 안에 있을 때만 엔딩 시작
			APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
			if (PlayerPawn && TriggerBox && TriggerBox->IsOverlappingActor(PlayerPawn))
			{
				bEndingStarted = true;
				StartEnding();
			}
			else
			{
				// 플레이어가 없는데 탑승 상태로 남아있으면 상태 초기화
				bPlayerBoarded = false;
				BoardElapsed = 0.f;
			}
		}
	}
}

void AEndingElevator::OnEndingTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	// 맵 로드 직후 자동 발송되는 초기 오버랩은 무시 (패키징 빌드 오작동 방지)
	if (IsTriggerGraceActive()) return;

	if (bPlayerBoarded) return;

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (OtherActor && OtherActor == PlayerPawn)
	{
		bPlayerBoarded = true;
	}
}

void AEndingElevator::OnEndingTriggerEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (bEndingStarted || !bPlayerBoarded) return;

	// 문이 닫힌 뒤(이동 중 텔레포트 포함)에는 취소하지 않음
	if (State != EElevatorState::Idle &&
		State != EElevatorState::DoorOpening &&
		State != EElevatorState::Boarding)
		return;

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (OtherActor && OtherActor == PlayerPawn)
	{
		bPlayerBoarded = false;
		BoardElapsed = 0.f;
	}
}

void AEndingElevator::StartEnding()
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC) return;

    // ★ 크로스헤어 숨기기
    if (AmaterialCharacter* Char = Cast<AmaterialCharacter>(PC->GetPawn()))
    {
        Char->HideCrosshair();
    }

    // 브금 끄기
    if (TeleportAudioComp)
        TeleportAudioComp->Stop();

    // 카메라 암전
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

    // 입력 막기
    PC->SetInputMode(FInputModeUIOnly());

    // 텍스트 위젯
    FTimerHandle TextTimer;
    GetWorldTimerManager().SetTimer(TextTimer, this,
        &AEndingElevator::ShowEndingText, TextDelay, false);

    // 메인 메뉴 이동
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