#include "TransformPowerTrigger.h"

ATransformPowerTrigger::ATransformPowerTrigger()
{
    PrimaryActorTick.bCanEverTick = false;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void ATransformPowerTrigger::BeginPlay()
{
    Super::BeginPlay();
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATransformPowerTrigger::OnOverlapBegin);
}

void ATransformPowerTrigger::OnOverlapBegin(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!OtherActor) return;

    ATransformation_actor* OverlappedTransformActor = Cast<ATransformation_actor>(OtherActor);
    
    if (OverlappedTransformActor)
    {
        if (TargetBattery && !TargetBattery->bPowered)
        {
            TargetBattery->TogglePower();
            
            UE_LOG(LogTemp, Warning, TEXT("Transformation_actor가 감지되어 배터리가 켜졌습니다!"));
        }
    }
}