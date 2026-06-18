#include "TransformPowerTrigger.h"
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"   // ★ 추가
#include "Sound/SoundBase.h"              // ★ 추가
#include "Kismet/GameplayStatics.h"       // ★ 추가
#include "TimerManager.h"                 // ★ 추가

ATransformPowerTrigger::ATransformPowerTrigger()
{
    PrimaryActorTick.bCanEverTick = true;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));

    // ★ 페달 소리 로드
    static ConstructorHelpers::FObjectFinder<USoundBase> PedalAsset(
        TEXT("/Script/Engine.SoundWave'/Game/Sound/sound_pedal.sound_pedal'"));
    if (PedalAsset.Succeeded())
        PedalSound = PedalAsset.Object;

    // ★ 문 열리는 소리 로드
    static ConstructorHelpers::FObjectFinder<USoundBase> DoorOpenAsset(
        TEXT("/Script/Engine.SoundWave'/Game/Sound/sound_door_opening.sound_door_opening'"));
    if (DoorOpenAsset.Succeeded())
        DoorOpenSound = DoorOpenAsset.Object;
}

void ATransformPowerTrigger::BeginPlay()
{
    Super::BeginPlay();

    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATransformPowerTrigger::OnOverlapBegin);

    // ── 문짝 시작/목표 위치 미리 계산 ──
    // 왼쪽 문: -Y 방향, 오른쪽 문: +Y 방향으로 OpenDistance만큼
    if (LeftDoorActor)
    {
        LeftStartLocation = LeftDoorActor->GetActorLocation();
        LeftTargetLocation = LeftStartLocation + FVector(0.f, -OpenDistance, 0.f);
    }
    if (RightDoorActor)
    {
        RightStartLocation = RightDoorActor->GetActorLocation();
        RightTargetLocation = RightStartLocation + FVector(0.f, OpenDistance, 0.f);
    }

    // ── 시작 시 이미 들어와 있는 액터 체크 ──
    TArray<AActor*> OverlappingActors;
    TriggerBox->GetOverlappingActors(OverlappingActors);
    for (AActor* Actor : OverlappingActors)
    {
        OnOverlapBegin(TriggerBox, Actor, nullptr, 0, false, FHitResult());
    }
}

void ATransformPowerTrigger::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ── 문 Lerp 열림 처리 ──
    if (bIsOpening && !bIsOpen)
    {
        CurrentTime += DeltaTime * OpenSpeed;
        const float Alpha = FMath::Clamp(CurrentTime, 0.f, 1.f);

        if (LeftDoorActor)
            LeftDoorActor->SetActorLocation(FMath::Lerp(LeftStartLocation, LeftTargetLocation, Alpha));
        if (RightDoorActor)
            RightDoorActor->SetActorLocation(FMath::Lerp(RightStartLocation, RightTargetLocation, Alpha));

        if (Alpha >= 1.f)
        {
            bIsOpen = true;
            bIsOpening = false;
        }
    }

    if (GEngine)
    {
        TArray<AActor*> OverlappingActors;
        TriggerBox->GetOverlappingActors(OverlappingActors);

        FString DebugMsg = FString::Printf(
            TEXT("=== 트리거 감지 상태 ===\n에어컨 상태: %s\n문 상태: %s\n"),
            (TargetAircon && TargetAircon->bIsRunning) ? TEXT("작동 중") : TEXT("대기 중"),
            bIsOpen ? TEXT("열림") : (bIsOpening ? TEXT("열리는 중") : TEXT("닫힘")));

        if (OverlappingActors.Num() == 0)
        {
            DebugMsg += TEXT("들어온 물체 없음\n");
        }
        else
        {
            for (AActor* Actor : OverlappingActors)
            {
                if (!Actor) continue;
                DebugMsg += FString::Printf(TEXT("▶ 액터 이름: %s\n"), *Actor->GetName());

                if (Actor->Tags.Num() > 0)
                {
                    DebugMsg += TEXT("  태그: ");
                    for (FName Tag : Actor->Tags)
                        DebugMsg += Tag.ToString() + TEXT(", ");
                    DebugMsg += TEXT("\n");
                }
                else
                {
                    DebugMsg += TEXT("  태그: 없음!\n");
                }
            }
        }

        GEngine->AddOnScreenDebugMessage(1, 0.1f, FColor::Yellow, DebugMsg);
    }
}

void ATransformPowerTrigger::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == this) return;

    if (OtherActor->ActorHasTag(KeyTag))
    {
        if (!TargetAircon)
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
                TEXT("[실패] 에러: 타겟 에어컨이 지정되지 않았습니다!"));
            return;
        }

        // ── 에어컨 ON ──
        if (TargetAircon->bIsRunning)
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange,
                TEXT("[무시] 에어컨이 이미 작동 중입니다."));
        }
        else
        {
            TargetAircon->ActivateAircon();
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
                TEXT("★ 키 블록 일치! 에어컨 ON!"));
        }

        // ── 문 열기 (아직 안 열렸으면) ──
        OpenDoor();
    }
}

void ATransformPowerTrigger::OpenDoor()
{
    if (bIsOpen || bIsOpening)
        return;   // 이미 열렸거나 열리는 중이면 무시

    bIsOpening = true;
    CurrentTime = 0.f;

    // ★ 1) 페달 소리 먼저 즉시 재생
    if (PedalSound)
        UGameplayStatics::PlaySoundAtLocation(this, PedalSound, GetActorLocation());

    // ★ 2) 문 소리는 DoorSoundDelay초 뒤에 재생 (페달 소리가 먼저 들리도록)
    if (DoorOpenSound)
    {
        const FVector SoundLoc = GetActorLocation();
        USoundBase* SoundToPlay = DoorOpenSound;
        GetWorld()->GetTimerManager().SetTimer(DoorSoundTimerHandle,
            [this, SoundToPlay, SoundLoc]()
            {
                UGameplayStatics::PlaySoundAtLocation(this, SoundToPlay, SoundLoc);
            },
            DoorSoundDelay, false);
    }

    // 열리는 동안 문 충돌 끄기 (캐릭터가 통과 가능하도록)
    if (LeftDoorActor)
        LeftDoorActor->SetActorEnableCollision(false);
    if (RightDoorActor)
        RightDoorActor->SetActorEnableCollision(false);

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
        TEXT("▶ 문 열림 시작!"));
}