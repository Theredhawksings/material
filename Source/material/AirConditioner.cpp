#include "AirConditioner.h"
#include "Engine/Engine.h"

AAirConditioner::AAirConditioner()
{
    PrimaryActorTick.bCanEverTick = true;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    RootComponent = MeshComp;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(
        TEXT("/Game/modeling/Object/air_condition/air_conditioning.air_conditioning"));
    if (MeshFinder.Succeeded())
        MeshComp->SetStaticMesh(MeshFinder.Object);

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
        TEXT("/Game/modeling/Object/air_condition/M_Air_condition.M_Air_condition"));
    if (MatFinder.Succeeded())
        MeshComp->SetMaterial(0, MatFinder.Object);

    MeshComp->SetCollisionProfileName(TEXT("BlockAll"));
    MeshComp->SetRelativeScale3D(FVector(300.f, 300.f, 300.f));

    DetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionBox"));
    DetectionBox->SetupAttachment(MeshComp);
    DetectionBox->SetBoxExtent(FVector(0.005f, 0.005f, 0.005f));
    DetectionBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AAirConditioner::BeginPlay()
{
    Super::BeginPlay();

    // ── bAlwaysOn 이면 시작하자마자 ON ──
    if (bAlwaysOn)
    {
        ActivateAircon();
        return;
    }

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
            TEXT("=== 에어컨 상태 ===\n작동 중: %s\n상시 활성화: %s\n"),
            bIsRunning ? TEXT("ON") : TEXT("OFF"),
            bAlwaysOn ? TEXT("ON") : TEXT("OFF"));

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
    // ── bAlwaysOn 이면 끄지 않음 ──
    if (bAlwaysOn)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
            TEXT("[에어컨] 상시 활성화 모드 - 끄기 불가"));
        return;
    }

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