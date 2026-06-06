#include "TransformPowerTrigger.h"
#include "Engine/Engine.h"

ATransformPowerTrigger::ATransformPowerTrigger()
{
    PrimaryActorTick.bCanEverTick = true;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void ATransformPowerTrigger::BeginPlay()
{
    Super::BeginPlay();

    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATransformPowerTrigger::OnOverlapBegin);

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

    if (GEngine)
    {
        TArray<AActor*> OverlappingActors;
        TriggerBox->GetOverlappingActors(OverlappingActors);

        FString DebugMsg = FString::Printf(
            TEXT("=== 트리거 감지 상태 ===\n에어컨 상태: %s\n"),
            (TargetAircon && TargetAircon->bIsRunning) ? TEXT("작동 중") : TEXT("대기 중"));

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

        // ── 한 번 들어오면 무조건 ON (이미 켜져있어도 무시하지 않고 그냥 ON 유지) ──
        if (TargetAircon->bIsRunning)
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange,
                TEXT("[무시] 에어컨이 이미 작동 중입니다."));
            return;
        }

        TargetAircon->ActivateAircon();
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
            TEXT("★ 키 블록 일치! 에어컨 ON!"));
    }
}