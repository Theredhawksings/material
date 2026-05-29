#include "BatteryOffTrigger.h"
#include "GameFramework/Character.h"
#include "Generator.h"
#include "IronSpawner.h"

ABatteryOffTrigger::ABatteryOffTrigger()
{
    PrimaryActorTick.bCanEverTick = false;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void ABatteryOffTrigger::BeginPlay()
{
    Super::BeginPlay();
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ABatteryOffTrigger::OnOverlapBegin);
}

void ABatteryOffTrigger::OnOverlapBegin(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    ACharacter* OverlappedCharacter = Cast<ACharacter>(OtherActor);
    if (!OverlappedCharacter || !OverlappedCharacter->IsPlayerControlled()) return;

    // 배터리 전원 토글
    if (TargetBattery && TargetBattery->bPowered)
        TargetBattery->TogglePower();

    // 발전기 작동
    if (TargetGenerator)
        TargetGenerator->ActivateGenerator();

    // 스포너 작동
    if (TargetSpawner)
        TargetSpawner->ActivateSpawner();
}