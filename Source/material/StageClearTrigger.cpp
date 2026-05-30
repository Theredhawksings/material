#include "StageClearTrigger.h"
#include "GameFramework/Character.h"
#include "Generator.h"
#include "IronSpawner.h"

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
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}