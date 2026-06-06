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

    // ── 얼음 올려놓는 감지 박스 (디버깅 용도) ──
    DetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionBox"));
    DetectionBox->SetupAttachment(MeshComp);
    DetectionBox->SetBoxExtent(FVector(0.005f, 0.005f, 0.005f));
    DetectionBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AAirConditioner::BeginPlay()
{
    Super::BeginPlay();

    DetectionBox->OnComponentBeginOverlap.AddDynamic(this, &AAirConditioner::OnDetectionOverlapBegin);
    DetectionBox->OnComponentEndOverlap.AddDynamic(this, &AAirConditioner::OnDetectionOverlapEnd);

    // ── bAlwaysOn 이면 시작하자마자 ON (스위치 불필요) ──
    if (bAlwaysOn)
    {
        ActivateAircon();
    }
}

void AAirConditioner::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (GEngine && DetectionBox)
    {
        TArray<AActor*> OverlappingActors;
        DetectionBox->GetOverlappingActors(OverlappingActors);

        // 박스 위에 얼음이 있는지 확인
        bool bIceDetected = false;
        for (AActor* Actor : OverlappingActors)
        {
            if (Actor && Actor->ActorHasTag(TEXT("Ice")))
            {
                bIceDetected = true;
                break;
            }
        }

        FString DebugMsg = FString::Printf(
            TEXT("=== 에어컨 상태 ===\n작동: %s\n상시활성화: %s\n박스 위 얼음: %s\n감지된 액터 수: %d\n"),
            bIsRunning ? TEXT("ON") : TEXT("OFF"),
            bAlwaysOn ? TEXT("ON") : TEXT("OFF"),
            bIceDetected ? TEXT("있음") : TEXT("없음"),
            OverlappingActors.Num());

        for (AActor* Actor : OverlappingActors)
        {
            if (!Actor) continue;
            DebugMsg += FString::Printf(TEXT("▶ %s\n"), *Actor->GetName());
        }

        GEngine->AddOnScreenDebugMessage(2, 0.1f, FColor::Cyan, DebugMsg);
    }
}

void AAirConditioner::OnDetectionOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == this) return;

    // ── 감지만 표시, 에어컨은 켜지 않음 ──
    if (OtherActor->ActorHasTag(TEXT("Ice")))
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
            FString::Printf(TEXT("[감지] 얼음 진입: %s"), *OtherActor->GetName()));
    }
}

void AAirConditioner::OnDetectionOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!OtherActor || OtherActor == this) return;

    if (OtherActor->ActorHasTag(TEXT("Ice")))
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::White,
            FString::Printf(TEXT("[감지] 얼음 퇴장: %s"), *OtherActor->GetName()));
    }
}

void AAirConditioner::ActivateAircon()
{
    if (bIsRunning) return;

    bIsRunning = true;

    // 차가운 바람 로직은 아직 미구현

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
        TEXT("[에어컨] 작동 시작 (ON)"));
}

void AAirConditioner::DeactivateAircon()
{
    if (bAlwaysOn) return;
    if (!bIsRunning) return;

    bIsRunning = false;

    // 차가운 바람 로직은 아직 미구현

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::White,
        TEXT("[에어컨] 작동 정지 (OFF)"));
}