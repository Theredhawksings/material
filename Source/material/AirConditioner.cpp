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

    DetectionBox->OnComponentBeginOverlap.AddDynamic(this, &AAirConditioner::OnDetectionOverlapBegin);
    DetectionBox->OnComponentEndOverlap.AddDynamic(this, &AAirConditioner::OnDetectionOverlapEnd);

    TArray<AActor*> OverlappingActors;
    DetectionBox->GetOverlappingActors(OverlappingActors);
    for (AActor* Actor : OverlappingActors)
    {
        OnDetectionOverlapBegin(DetectionBox, Actor, nullptr, 0, false, FHitResult());
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
        TArray<AActor*> OverlappingActors;
        DetectionBox->GetOverlappingActors(OverlappingActors);

        FString DebugMsg = FString::Printf(
            TEXT("=== 에어컨 상태 ===\n작동 중: %s\n감지된 액터 수: %d\n"),
            bIsRunning ? TEXT("ON") : TEXT("OFF"),
            OverlappingActors.Num());

        for (AActor* Actor : OverlappingActors)
        {
            if (!Actor) continue;
            DebugMsg += FString::Printf(TEXT("▶ %s\n"), *Actor->GetName());

            if (Actor->Tags.Num() > 0)
            {
                DebugMsg += TEXT("  태그: ");
                for (FName Tag : Actor->Tags)
                    DebugMsg += Tag.ToString() + TEXT(", ");
                DebugMsg += TEXT("\n");
            }
            else
            {
                DebugMsg += TEXT("  태그: 없음\n");
            }
        }

        GEngine->AddOnScreenDebugMessage(2, 0.1f, FColor::Cyan, DebugMsg);
    }
}

void AAirConditioner::OnDetectionOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == this) return;

    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
            FString::Printf(TEXT("[에어컨] 감지: %s"), *OtherActor->GetName()));

    if (OtherActor->ActorHasTag(TEXT("Ice")))
    {
        ActivateAircon();
    }
    else
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
                TEXT("[에어컨] Ice 태그 없음 - 무시"));
    }
}

void AAirConditioner::OnDetectionOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!OtherActor || OtherActor == this) return;

    if (OtherActor->ActorHasTag(TEXT("Ice")))
    {
        DeactivateAircon();
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