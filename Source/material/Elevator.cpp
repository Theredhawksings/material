// Fill out your copyright notice in the Description page of Project Settings.

#include "Elevator.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"

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

    // --- 메시 자동 로드 ---
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

    // --- 머티리얼 자동 로드 ---
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BodyMat(
        TEXT("/Game/modeling/Object/elevator/M_Elevator_Body.M_Elevator_Body"));
    if (BodyMat.Succeeded())
    {
        Body->SetMaterial(0, BodyMat.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> DoorMat(
        TEXT("/Game/modeling/Object/elevator/elevator_Door_texture_Mat.elevator_Door_texture_Mat"));
    if (DoorMat.Succeeded())
    {
        Door1->SetMaterial(0, DoorMat.Object);
        DoorL->SetMaterial(0, DoorMat.Object);
        DoorR->SetMaterial(0, DoorMat.Object);
    }

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
    {
        // 실시간 갱신 시 화면 도배를 막기 위해 키값을 1로 고정
        GEngine->AddOnScreenDebugMessage(1, 3.f, Color, FString::Printf(TEXT("[Elevator] %s"), *Msg));
    }
}

void AElevator::BeginPlay()
{
    Super::BeginPlay();

    SetDoorYaw(DoorClosedYaw);
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AElevator::OnTriggerBegin);

    DebugMsg(FString::Printf(TEXT("BeginPlay. DoorL mesh=%s"),
        DoorL && DoorL->GetStaticMesh() ? TEXT("OK") : TEXT("NULL")), FColor::Cyan);
}

void AElevator::SetDoorYaw(float Yaw)
{
    // [수정] 양쪽 문이 대칭으로 열리도록 처리
    if (DoorL)
    {
        DoorL->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
    }
    if (DoorR)
    {
        DoorR->SetRelativeRotation(FRotator(0.f, -Yaw, 0.f)); 
    }
}

void AElevator::OnTriggerBegin(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
    UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/,
    bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
    if (State != EElevatorState::Idle)
    {
        return; // 작동 중 무시
    }

    if (!Cast<ACharacter>(OtherActor) && !Cast<APawn>(OtherActor))
    {
        return;
    }

    Passenger = OtherActor;
    State = EElevatorState::DoorOpening;
    PhaseElapsed = 0.f;
    DebugMsg(TEXT(">>> 탑승 확인! 문 열기 시작"), FColor::Green);
}

void AElevator::CloseDoors()
{
    State = EElevatorState::DoorClosing;
    PhaseElapsed = 0.f;
    DebugMsg(TEXT(">>> 탑승 완료. 문 닫기 시작"), FColor::Green);
}

void AElevator::TeleportPlayer()
{
    if (Passenger)
    {
        // 1. 플레이어 위치 차이 계산 후, 바닥에 끼지 않도록 Z축(높이)을 50.f 정도 띄워줍니다.
        FVector PlayerOffset = Passenger->GetActorLocation() - this->GetActorLocation();
        PlayerOffset.Z += 50.f; 

        // 2. 엘리베이터 본체 먼저 이동
        this->SetActorLocation(DestinationLocation, false, nullptr, ETeleportType::TeleportPhysics);

        // 3. 플레이어 이동 (SetActorLocation 대신 캐릭터 이동에 특화된 TeleportTo 사용!)
        FVector TargetPlayerLoc = this->GetActorLocation() + PlayerOffset;
        Passenger->TeleportTo(TargetPlayerLoc, Passenger->GetActorRotation(), false, true);

        DebugMsg(TEXT(">>> 플레이어 동반 텔레포트 완료!"), FColor::Magenta);
    }
    else
    {
        DebugMsg(TEXT(">>> 탑승객을 잃어버렸습니다!"), FColor::Red);
    }

    // 4. 도착지에서 문 열기 상태로 전환
    State = EElevatorState::ArrivalOpening;
    PhaseElapsed = 0.f;
}

void AElevator::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // 문이 움직일 때만 시간 누적
    if (State == EElevatorState::DoorOpening || State == EElevatorState::DoorClosing || State == EElevatorState::ArrivalOpening)
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
            State = EElevatorState::Done;
            // [수정] TravelTime(기본 3초) 동안 대기한 뒤 TeleportPlayer 호출하여 '이동하는 느낌' 부여
            GetWorldTimerManager().SetTimer(TeleportTimer, this, &AElevator::TeleportPlayer, TravelTime, false);
            DebugMsg(TEXT("문 닫힘 완료 → 목적지로 이동 중..."), FColor::Yellow);
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
            Passenger = nullptr;
            
            // [수정] 다시 Idle로 돌아가지 않고 Disabled 상태로 고정시켜 버립니다!
            State = EElevatorState::Disabled; 
            
            DebugMsg(TEXT("도착지 문 개방 완료! 엘리베이터 영구 정지됨"), FColor::Red);
        }
        break;
    }
    default:
        break;
    }
}