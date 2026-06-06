#include "AirConditioner.h"
#include "Engine/Engine.h"

AAirConditioner::AAirConditioner()
{
    PrimaryActorTick.bCanEverTick = true;

    // ── 메시 생성 및 루트 설정 ──
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    RootComponent = MeshComp;

    // ── 메시 에셋 C++에서 지정 ──
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(
        TEXT("/Game/modeling/Object/air_condition/air_conditioning.air_conditioning"));
    if (MeshFinder.Succeeded())
        MeshComp->SetStaticMesh(MeshFinder.Object);

    // ── 머터리얼 C++에서 지정 ──
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
        TEXT("/Game/modeling/Object/air_condition/M_Air_condition.M_Air_condition"));
    if (MatFinder.Succeeded())
        MeshComp->SetMaterial(0, MatFinder.Object);

    MeshComp->SetCollisionProfileName(TEXT("BlockAll"));
    MeshComp->SetRelativeScale3D(FVector(100.f, 100.f, 100.f));  // ← 생성자에서 설정

    // ── 감지 박스 생성 ──
    DetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionBox"));
    DetectionBox->SetupAttachment(MeshComp);
    DetectionBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));
    DetectionBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AAirConditioner::BeginPlay()
{
    Super::BeginPlay();

    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
            TEXT("[에어컨] 초기화 완료 - 대기 중"));
}

void AAirConditioner::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (GEngine)
    {
        FString DebugMsg = FString::Printf(
            TEXT("=== 에어컨 상태 ===\n작동 중: %s\n"),
            bIsRunning ? TEXT("ON") : TEXT("OFF"));

        GEngine->AddOnScreenDebugMessage(2, 0.1f, FColor::Cyan, DebugMsg);
    }
}

void AAirConditioner::ActivateAircon()
{
    if (bIsRunning)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
            TEXT("[에어컨] 이미 작동 중입니다"));
        return;
    }

    bIsRunning = true;

    // 나중에 차가운 바람 ON 구현 예정

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
        TEXT("[에어컨] 작동 시작!"));
}

void AAirConditioner::DeactivateAircon()
{
    if (!bIsRunning)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
            TEXT("[에어컨] 이미 꺼져 있습니다"));
        return;
    }

    bIsRunning = false;

    // 나중에 차가운 바람 OFF 구현 예정

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::White,
        TEXT("[에어컨] 작동 정지"));
}