#include "BatteryOffTrigger.h"
#include "GameFramework/Character.h"

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
    if (!Cast<ACharacter>(OtherActor)) return;
    if (!Cast<ACharacter>(OtherActor)->IsPlayerControlled()) return;

    if (TargetBattery && TargetBattery->bPowered)
        TargetBattery->TogglePower();
}