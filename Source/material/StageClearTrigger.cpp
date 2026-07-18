#include "StageClearTrigger.h"
#include "GameFramework/Character.h"
#include "Generator.h"
#include "IronSpawner.h"
#include "AirConditioner.h"   // ★ 추가

AStageClearTrigger::AStageClearTrigger()
{
    PrimaryActorTick.bCanEverTick = false;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetBoxExtent(FVector(150.f, 150.f, 100.f));
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AStageClearTrigger::BeginPlay()
{
    Super::BeginPlay();
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AStageClearTrigger::OnOverlapBegin);
}

void AStageClearTrigger::OnOverlapBegin(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    ACharacter* OverlappedCharacter = Cast<ACharacter>(OtherActor);
    if (!OverlappedCharacter || !OverlappedCharacter->IsPlayerControlled()) return;

    if (TargetGenerator)
    {
        TargetGenerator->DeactivateGenerator();
    }

    if (TargetSpawner)
    {
        TargetSpawner->DeactivateSpawner();
    }

    // ★ 에어컨 멈추기 (상시 작동이어도 강제로 끔)
    if (TargetAircon)
    {
        TargetAircon->DeactivateAircon(true);
    }

    TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}