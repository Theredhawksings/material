#include "Elevator.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

AElevator::AElevator()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    Body  = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    Door1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Door1"));
    DoorL = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorL"));
    DoorR = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorR"));

    Body->SetupAttachment(SceneRoot);
    Door1->SetupAttachment(Body);
    DoorL->SetupAttachment(Body);
    DoorR->SetupAttachment(Body);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(
        TEXT("/Game/modeling/Object/elevator/Elevator_Body.Elevator_Body"));
    if (BodyMesh.Succeeded()) Body->SetStaticMesh(BodyMesh.Object);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Door1Mesh(
        TEXT("/Game/modeling/Object/elevator/Elevator_Door1.Elevator_Door1"));
    if (Door1Mesh.Succeeded()) Door1->SetStaticMesh(Door1Mesh.Object);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> DoorLMesh(
        TEXT("/Game/modeling/Object/elevator/Elevator_Door2_L.Elevator_Door2_L"));
    if (DoorLMesh.Succeeded()) DoorL->SetStaticMesh(DoorLMesh.Object);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> DoorRMesh(
        TEXT("/Game/modeling/Object/elevator/Elevator_Door2_R.Elevator_Door2_R"));
    if (DoorRMesh.Succeeded()) DoorR->SetStaticMesh(DoorRMesh.Object);

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BodyMat(
        TEXT("/Game/modeling/Object/elevator/M_Elevator_Body.M_Elevator_Body"));
    if (BodyMat.Succeeded())
        Body->SetMaterial(0, BodyMat.Object);

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> DoorMat(
        TEXT("/Game/modeling/Object/elevator/elevator_Door_texture_Mat.elevator_Door_texture_Mat"));
    if (DoorMat.Succeeded())
    {
        Door1->SetMaterial(0, DoorMat.Object);
        DoorL->SetMaterial(0, DoorMat.Object);
        DoorR->SetMaterial(0, DoorMat.Object);
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> OpenSnd(
        TEXT("/Game/Sound/sound_elevator_openning.sound_elevator_openning"));
    if (OpenSnd.Succeeded()) OpenSound = OpenSnd.Object;

    static ConstructorHelpers::FObjectFinder<USoundBase> CloseSnd(
        TEXT("/Game/Sound/sound_elevator_closing.sound_elevator_closing"));
    if (CloseSnd.Succeeded()) CloseSound = CloseSnd.Object;

    static ConstructorHelpers::FObjectFinder<USoundBase> TeleportSnd(
        TEXT("SoundWave'/Game/Sound/sound_generator_turning_on.sound_generator_turning_on'"));
    if (TeleportSnd.Succeeded()) TeleportSound = TeleportSnd.Object;

    TeleportAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("TeleportAudioComp"));
    TeleportAudioComp->SetupAttachment(SceneRoot);
    TeleportAudioComp->bAutoActivate = false;
    if (TeleportSound)
        TeleportAudioComp->SetSound(TeleportSound);

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    TriggerBox->SetupAttachment(SceneRoot);
    TriggerBox->SetBoxExtent(FVector(250.f, 250.f, 120.f));
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
    TriggerBox->SetCollisionResponseToAllChannels(ECR_Overlap);
    TriggerBox->SetGenerateOverlapEvents(true);

    // 중앙 탑승 확인용 박스 - 기본값은 작게 잡아두고 에디터에서 내부 바닥 크기에 맞게 조정
    BoardingBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BoardingBox"));
    BoardingBox->SetupAttachment(SceneRoot);
    BoardingBox->SetBoxExtent(FVector(100.f, 100.f, 120.f));
    BoardingBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BoardingBox->SetCollisionObjectType(ECC_WorldDynamic);
    BoardingBox->SetCollisionResponseToAllChannels(ECR_Overlap);
    BoardingBox->SetGenerateOverlapEvents(true);
}

void AElevator::DebugMsg(const FString& Msg, const FColor& Color)
{
    if (!bDebug) return;
    UE_LOG(LogTemp, Warning, TEXT("[Elevator] %s"), *Msg);
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(1, 3.f, Color, FString::Printf(TEXT("[Elevator] %s"), *Msg));
}

void AElevator::BeginPlay()
{
    Super::BeginPlay();
    BeginPlayTimeSeconds = GetWorld()->GetTimeSeconds();

    // 에디터에서 배치한 문의 원래 회전을 닫힘 기준값으로 저장
    // (이걸 기준으로 열림/닫힘 각도를 얹어야 닫힐 때 원래 모습으로 정확히 복구됨)
    if (DoorL) DoorLBaseQuat = DoorL->GetRelativeRotation().Quaternion();
    if (DoorR) DoorRBaseQuat = DoorR->GetRelativeRotation().Quaternion();

    SetDoorYaw(DoorClosedYaw);
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AElevator::OnTriggerBegin);
    BoardingBox->OnComponentBeginOverlap.AddDynamic(this, &AElevator::OnBoardingBoxBegin);
    BoardingBox->OnComponentEndOverlap.AddDynamic(this, &AElevator::OnBoardingBoxEnd);
    DebugMsg(FString::Printf(TEXT("BeginPlay. DoorL mesh=%s"),
        DoorL && DoorL->GetStaticMesh() ? TEXT("OK") : TEXT("NULL")), FColor::Cyan);
}

void AElevator::SetDoorYaw(float Yaw)
{
    // 에디터 배치 회전(Base) 위에 열림 각도를 오프셋으로 얹음
    // Yaw=0(닫힘)이면 정확히 원래 배치 모습으로 복구됨
    // DoorL은 +Yaw, DoorR은 -Yaw 방향으로 회전
    const float Rad = FMath::DegreesToRadians(Yaw);
    if (DoorL) DoorL->SetRelativeRotation((DoorLBaseQuat * FQuat(FVector::UpVector, Rad)).Rotator());
    if (DoorR) DoorR->SetRelativeRotation((DoorRBaseQuat * FQuat(FVector::UpVector, -Rad)).Rotator());
}

void AElevator::PlayOpenSound()
{
    if (OpenSound)
        UGameplayStatics::PlaySoundAtLocation(this, OpenSound, GetActorLocation());
}

void AElevator::PlayCloseSound()
{
    if (CloseSound)
        UGameplayStatics::PlaySoundAtLocation(this, CloseSound, GetActorLocation());
}

bool AElevator::IsTriggerGraceActive() const
{
    return GetWorld()->GetTimeSeconds() - BeginPlayTimeSeconds < TriggerGraceTime;
}

void AElevator::OnTriggerBegin(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
    UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/,
    bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
    // 맵 로드 직후 자동 발송되는 초기 오버랩은 무시
    if (IsTriggerGraceActive())
    {
        DebugMsg(TEXT("맵 로드 직후 오버랩 무시 (grace)"), FColor::Orange);
        return;
    }

    if (!Cast<ACharacter>(OtherActor) && !Cast<APawn>(OtherActor)) return;

    if (State == EElevatorState::Idle)
    {
        Passenger = OtherActor;
        State = EElevatorState::DoorOpening;
        PhaseElapsed = 0.f;

        GetWorldTimerManager().SetTimer(SoundTimer, this, &AElevator::PlayOpenSound, SoundDelay, false);
        DebugMsg(TEXT(">>> 근처 접근 감지! 문 열기 시작"), FColor::Green);
    }
    // Boarding 상태에서는 BoardingBox가 탑승 확정을 담당하므로 여기서는 아무것도 안 함
}

void AElevator::OnBoardingBoxBegin(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
    UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/,
    bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
    // 문이 열려 대기 중일 때 실제로 중앙(탑승 공간)에 들어온 경우에만 탑승 확정
    if (State == EElevatorState::Boarding && !bBoardConfirmed && OtherActor == Passenger)
    {
        OnBoardingConfirmed();
    }
}

void AElevator::OnBoardingBoxEnd(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
    UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
    if (State != EElevatorState::Boarding) return;
    if (!bBoardConfirmed || OtherActor != Passenger) return;

    // 문 닫히기 전에 다시 나감 - 탑승 취소, 계속 대기
    GetWorldTimerManager().ClearTimer(BoardConfirmTimer);
    bBoardConfirmed = false;

    // 탑승 확정 때 껐던 타임아웃 타이머를 다시 켬
    // (안 켜면 플레이어가 나간 뒤 문이 영원히 열린 채로 대기하게 됨)
    GetWorldTimerManager().SetTimer(BoardTimer, this, &AElevator::OnBoardingTimeout, BoardingTime, false);
    DebugMsg(TEXT(">>> 탑승 취소됨 - 다시 대기 중..."), FColor::Orange);
}

void AElevator::OnBoardingConfirmed()
{
    if (bBoardConfirmed) return;
    bBoardConfirmed = true;

    GetWorldTimerManager().ClearTimer(BoardTimer);
    GetWorldTimerManager().SetTimer(BoardConfirmTimer, this, &AElevator::CloseDoors, PostBoardCloseDelay, false);
    DebugMsg(FString::Printf(TEXT(">>> 탑승 확정! %.1f초 후 문 닫힘"), PostBoardCloseDelay), FColor::Green);
}

void AElevator::OnBoardingTimeout()
{
    if (bBoardConfirmed) return;

    Passenger = nullptr;
    CloseDoors();
    DebugMsg(TEXT(">>> 탑승 대기 시간 초과 - 아무도 타지 않음, 문 닫음"), FColor::Orange);
}

void AElevator::CloseDoors()
{
    State = EElevatorState::DoorClosing;
    PhaseElapsed = 0.f;

    GetWorldTimerManager().SetTimer(SoundTimer, this, &AElevator::PlayCloseSound, SoundDelay, false);
    DebugMsg(TEXT(">>> 탑승 완료. 문 닫기 시작"), FColor::Green);
}

void AElevator::TeleportPlayer()
{
    // ★ 도착 순간 이동 사운드 즉시 끔
    if (TeleportAudioComp && TeleportAudioComp->IsPlaying())
        TeleportAudioComp->Stop();

    if (Passenger)
    {
        FVector PlayerOffset = Passenger->GetActorLocation() - GetActorLocation();
        PlayerOffset.Z += 50.f;

        SetActorLocation(DestinationLocation, false, nullptr, ETeleportType::TeleportPhysics);
        OriginalLocation = DestinationLocation;

        FVector TargetPlayerLoc = GetActorLocation() + PlayerOffset;
        Passenger->TeleportTo(TargetPlayerLoc, Passenger->GetActorRotation(), false, true);

        DebugMsg(TEXT(">>> 플레이어 동반 텔레포트 완료!"), FColor::Magenta);
    }
    else
    {
        DebugMsg(TEXT(">>> 탑승객을 잃어버렸습니다!"), FColor::Red);
    }

    State = EElevatorState::ArrivalOpening;
    PhaseElapsed = 0.f;

    GetWorldTimerManager().SetTimer(SoundTimer, this, &AElevator::PlayOpenSound, SoundDelay, false);
}

void AElevator::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (State == EElevatorState::DoorOpening ||
        State == EElevatorState::DoorClosing ||
        State == EElevatorState::ArrivalOpening)
    {
        PhaseElapsed += DeltaTime;
    }

    switch (State)
    {
    case EElevatorState::DoorOpening:
    {
        const float A = (DoorMoveDuration > 0.f)
            ? FMath::Clamp(PhaseElapsed / DoorMoveDuration, 0.f, 1.f) : 1.f;
        SetDoorYaw(FMath::Lerp(DoorClosedYaw, DoorOpenYaw, A));
        if (A >= 1.f)
        {
            // ★ 문 열림 완료 → 탑승 대기 (Boarding). 실제 탑승 확인 전엔 확정하지 않음
            State = EElevatorState::Boarding;
            bBoardConfirmed = false;

            if (Passenger && BoardingBox->IsOverlappingActor(Passenger))
            {
                // 문이 열리는 동안 이미 중앙 탑승 공간에 있었던 경우 - 바로 탑승 확정
                OnBoardingConfirmed();
            }
            else
            {
                // 아직 안 탐 - 최대 BoardingTime 만큼 탑승을 기다림
                GetWorldTimerManager().SetTimer(BoardTimer, this, &AElevator::OnBoardingTimeout, BoardingTime, false);
                DebugMsg(TEXT("문 열림 완료 → 탑승 대기 중..."), FColor::White);
            }
        }
        break;
    }
    case EElevatorState::DoorClosing:
    {
        const float A = (DoorMoveDuration > 0.f)
            ? FMath::Clamp(PhaseElapsed / DoorMoveDuration, 0.f, 1.f) : 1.f;
        SetDoorYaw(FMath::Lerp(DoorOpenYaw, DoorClosedYaw, A));
        if (A >= 1.f)
        {
            // ★ 문 닫힘 완료 직전 최종 확인: 탑승객이 실제로 안에 있을 때만 이동 시작
            const bool bPassengerPresent = Passenger && BoardingBox->IsOverlappingActor(Passenger);
            if (bPassengerPresent)
            {
                // ★ 이동 시작 (Done). 여기서부터 진동
                State = EElevatorState::Done;
                ShakeElapsed = 0.f;
                OriginalLocation = GetActorLocation();

                // ★ 출발 0.3초 뒤 이동 사운드 재생
                GetWorldTimerManager().SetTimer(SoundTimer, [this]()
                {
                    if (TeleportAudioComp && TeleportSound)
                    {
                        TeleportAudioComp->SetSound(TeleportSound);
                        TeleportAudioComp->SetVolumeMultiplier(0.4f);
                        TeleportAudioComp->Play();
                    }
                }, 0.3f, false);

                GetWorldTimerManager().SetTimer(TeleportTimer, this, &AElevator::TeleportPlayer, TravelTime, false);
                DebugMsg(TEXT("문 닫힘 완료 → 목적지로 이동 중..."), FColor::Yellow);
            }
            else
            {
                // 탑승객이 없음 - 이동 취소하고 대기 상태로 복귀
                Passenger = nullptr;
                State = EElevatorState::Idle;
                DebugMsg(TEXT("문 닫힘 완료 → 탑승객 없음, 이동 취소"), FColor::Orange);
            }
        }
        break;
    }
    case EElevatorState::Done:
    {
        // ★ 이동 중 진동 (세기 ShakeIntensity 기본 3.0)
        ShakeElapsed += DeltaTime;
        const float T = ShakeElapsed;
        const float I = ShakeIntensity;

        FVector ShakeOffset = FVector::ZeroVector;

        // 출발 눌림 (0~0.4초): 아래로 눌렸다가 복귀
        if (T < 0.4f)
        {
            ShakeOffset.Z = -4.f * I * FMath::Sin(T / 0.4f * PI);
        }
        // 이동 중: Z + 좌우 흔들림 (세기 상향)
        else if (T < TravelTime - 0.3f)
        {
            ShakeOffset.Z = I * (1.5f * FMath::Sin(T * 7.3f)
                              + 0.6f * FMath::Sin(T * 13.1f)
                              + 0.3f * FMath::Sin(T * 19.7f));
            ShakeOffset.X = I * (0.8f * FMath::Sin(T * 5.7f + 1.2f)
                              + 0.3f * FMath::Sin(T * 11.3f + 0.5f));
            ShakeOffset.Y = I * (0.6f * FMath::Sin(T * 8.9f + 0.7f)
                              + 0.2f * FMath::Sin(T * 15.1f + 1.1f));
        }
        // 도착 직전 (마지막 0.3초): 위로 튀었다가 안착
        else
        {
            const float Remain = TravelTime - T;
            ShakeOffset.Z = I * 3.f * FMath::Sin(Remain / 0.3f * PI);
        }

        // 엘리베이터 위치 적용
        SetActorLocation(OriginalLocation + ShakeOffset,
            false, nullptr, ETeleportType::TeleportPhysics);

        // 플레이어도 같이 흔들림
        if (Passenger)
        {
            const FVector PassengerBase = Passenger->GetActorLocation();
            Passenger->SetActorLocation(
                FVector(PassengerBase.X + ShakeOffset.X,
                        PassengerBase.Y + ShakeOffset.Y,
                        PassengerBase.Z + ShakeOffset.Z),
                false, nullptr, ETeleportType::TeleportPhysics);
        }
        break;
    }
    case EElevatorState::ArrivalOpening:
    {
        const float A = (DoorMoveDuration > 0.f)
            ? FMath::Clamp(PhaseElapsed / DoorMoveDuration, 0.f, 1.f) : 1.f;
        SetDoorYaw(FMath::Lerp(DoorClosedYaw, DoorOpenYaw, A));
        if (A >= 1.f)
        {
            // ★ 사운드는 TeleportPlayer()에서 이미 끔
            Passenger = nullptr;
            State = EElevatorState::Disabled;
            DebugMsg(TEXT("도착지 문 개방 완료! 엘리베이터 영구 정지됨"), FColor::Red);
        }
        break;
    }
    default:
        break;
    }
}