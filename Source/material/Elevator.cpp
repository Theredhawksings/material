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
    SetDoorYaw(DoorClosedYaw);
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AElevator::OnTriggerBegin);
    DebugMsg(FString::Printf(TEXT("BeginPlay. DoorL mesh=%s"),
        DoorL && DoorL->GetStaticMesh() ? TEXT("OK") : TEXT("NULL")), FColor::Cyan);
}

void AElevator::SetDoorYaw(float Yaw)
{
    if (DoorL) DoorL->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
    if (DoorR) DoorR->SetRelativeRotation(FRotator(0.f, -Yaw, 0.f));
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

    if (State != EElevatorState::Idle) return;
    if (!Cast<ACharacter>(OtherActor) && !Cast<APawn>(OtherActor)) return;

    Passenger = OtherActor;
    State = EElevatorState::DoorOpening;
    PhaseElapsed = 0.f;

    GetWorldTimerManager().SetTimer(SoundTimer, this, &AElevator::PlayOpenSound, SoundDelay, false);
    DebugMsg(TEXT(">>> 탑승 확인! 문 열기 시작"), FColor::Green);
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
            // ★ 문 열림 완료 → 탑승 대기 (Boarding). BoardingTime 뒤 문 닫힘
            State = EElevatorState::Boarding;
            GetWorldTimerManager().SetTimer(BoardTimer, this, &AElevator::CloseDoors, BoardingTime, false);
            DebugMsg(TEXT("문 열림 완료 → 대기 중..."), FColor::White);
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
            // ★ 문 닫힘 완료 → 이동 시작 (Done). 여기서부터 진동
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