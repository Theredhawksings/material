#include "WindModeToggleTrigger.h"
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"

AWindModeToggleTrigger::AWindModeToggleTrigger()
{
    PrimaryActorTick.bCanEverTick = false;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));

    // ★ 페달 소리 로드 (기존 발판과 동일)
    static ConstructorHelpers::FObjectFinder<USoundBase> PedalAsset(
        TEXT("/Script/Engine.SoundWave'/Game/Sound/sound_pedal.sound_pedal'"));
    if (PedalAsset.Succeeded())
        PedalSound = PedalAsset.Object;
}

void AWindModeToggleTrigger::BeginPlay()
{
    Super::BeginPlay();

    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AWindModeToggleTrigger::OnOverlapBegin);
}

void AWindModeToggleTrigger::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == this) return;

    if (!bAnyActorCanPress && !OtherActor->ActorHasTag(KeyTag)) return;

    if (TargetAircons.Num() == 0)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
            TEXT("[실패] 에러: 타겟 에어컨이 지정되지 않았습니다!"));
        return;
    }

    // ── 지정된 에어컨들의 바람 모드 전부 반전 ──
    for (AAirConditioner* Aircon : TargetAircons)
    {
        if (Aircon)
            Aircon->ToggleWindMode();
    }

    if (PedalSound)
        UGameplayStatics::PlaySoundAtLocation(this, PedalSound, GetActorLocation());

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
        TEXT("★ 바람 모드 발판! 에어컨 모드 반전!"));
}
