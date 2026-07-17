#include "CircuitSwitchPlate.h"
#include "Wire.h"
#include "Resistance.h"
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"

ACircuitSwitchPlate::ACircuitSwitchPlate()
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

void ACircuitSwitchPlate::BeginPlay()
{
    Super::BeginPlay();

    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACircuitSwitchPlate::OnOverlapBegin);

    // 시작 시 이미 올라와 있는 물체 체크
    TArray<AActor*> OverlappingActors;
    TriggerBox->GetOverlappingActors(OverlappingActors);
    for (AActor* Actor : OverlappingActors)
    {
        OnOverlapBegin(TriggerBox, Actor, nullptr, 0, false, FHitResult());
    }
}

void ACircuitSwitchPlate::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (bConnected) return; // 한 번 연결되면 끝
    if (!OtherActor || OtherActor == this) return;

    // 전선/저항 블럭은 발판을 누를 수 없음 (발판 주변에 항상 겹쳐 있으므로)
    if (Cast<AWire>(OtherActor) || Cast<AResistance>(OtherActor)) return;

    // 바닥 등 고정(Static) 액터도 제외 — 움직일 수 있는 물체만 누를 수 있음
    if (USceneComponent* RootC = OtherActor->GetRootComponent())
        if (RootC->Mobility != EComponentMobility::Movable) return;

    if (!bAnyActorCanPress && !OtherActor->ActorHasTag(KeyTag)) return;

    ConnectCircuit();
}

void ACircuitSwitchPlate::ConnectCircuit()
{
    if (!UpstreamWire || DownstreamWires.Num() == 0)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
            TEXT("[실패] 에러: 회로 발판에 전선이 지정되지 않았습니다!"));
        return;
    }

    bConnected = true;

    // ── 끊긴 구간을 영구 연결 ──
    for (const TObjectPtr<AWire>& Wire : DownstreamWires)
    {
        if (Wire)
            UpstreamWire->ManualDownstreamWires.AddUnique(Wire);
    }

    // 다음 주기 기다리지 않고 즉시 연결 갱신 (배터리 solve는 자체 주기로 따라옴)
    UpstreamWire->RefreshConnectedActors();

    if (PedalSound)
        UGameplayStatics::PlaySoundAtLocation(this, PedalSound, GetActorLocation());

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
        TEXT("★ 회로 발판! 전선 연결됨 — 전기가 흐릅니다"));
}
