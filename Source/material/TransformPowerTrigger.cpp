#include "TransformPowerTrigger.h"
#include "Engine/Engine.h" // 디버그 메시지 출력을 위해 추가

ATransformPowerTrigger::ATransformPowerTrigger()
{
    // 실시간 디버깅을 위해 Tick 활성화
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

        FString DebugMsg = TEXT("=== 현재 트리거 감지 상태 ===\n");

        if (OverlappingActors.Num() == 0)
        {
            DebugMsg += TEXT("들어온 물체 없음 (콜리전 설정 및 Overlap Event 확인 필요!)\n");
        }
        else
        {
            for (AActor* Actor : OverlappingActors)
            {
                if (!Actor) continue;

                DebugMsg += FString::Printf(TEXT("▶ 액터 이름: %s\n"), *Actor->GetName());

                if (Actor->Tags.Num() > 0)
                {
                    DebugMsg += TEXT("  적용된 태그: ");
                    for (FName Tag : Actor->Tags)
                    {
                        DebugMsg += Tag.ToString() + TEXT(", ");
                    }
                    DebugMsg += TEXT("\n");
                }
                else
                {
                    DebugMsg += TEXT("  적용된 태그: 없음!\n");
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
        if (!TargetBattery)
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[실패] 에러: 타겟 배터리가 지정되지 않았습니다! (에디터 확인 필요)"));
        }
        else if (TargetBattery->bPowered)
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange, TEXT("[무시] 주의: 지정된 배터리가 이미 켜져 있는 상태입니다."));
        }
        else
        {
            TargetBattery->TogglePower();
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("★ 열쇠 블록 일치! 배터리 전원 ON 성공!"));
        }
    }
}