#include "PressurePlate.h"
#include "Magnet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "UObject/ConstructorHelpers.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

APressurePlate::APressurePlate()
{
    PrimaryActorTick.bCanEverTick = false;   // ★ 더 이상 Tick 안 함

    PlateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlateMesh"));
    SetRootComponent(PlateMesh);
    PlateMesh->SetSimulatePhysics(false);

    DetectBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectBox"));
    DetectBox->SetupAttachment(PlateMesh);
    DetectBox->SetBoxExtent(FVector(60.f, 60.f, 20.f));
    DetectBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DetectBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    DetectBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    static ConstructorHelpers::FObjectFinder<USoundBase> PedalAsset(
        TEXT("/Script/Engine.SoundWave'/Game/Sound/sound_pedal.sound_pedal'"));
    if (PedalAsset.Succeeded())
        PedalSound = PedalAsset.Object;
}

void APressurePlate::BeginPlay()
{
    Super::BeginPlay();

    DetectBox->OnComponentBeginOverlap.AddDynamic(this, &APressurePlate::OnBoxBeginOverlap);
    DetectBox->OnComponentEndOverlap.AddDynamic(this,   &APressurePlate::OnBoxEndOverlap);

    // 자석 이동 초기화는 자석 본인이 BeginPlay에서 처리 → 여기선 없음
}

void APressurePlate::OnBoxBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!Cast<ACharacter>(OtherActor)) return;

    const bool bWasEmpty = OverlappingPlayers.IsEmpty();
    OverlappingPlayers.Add(OtherActor);

    if (bWasEmpty)
        ToggleMagnets();
}

void APressurePlate::OnBoxEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32)
{
    if (!Cast<ACharacter>(OtherActor)) return;
    OverlappingPlayers.Remove(OtherActor);
}

void APressurePlate::ToggleMagnets()
{
    if (PedalSound)
        UGameplayStatics::PlaySoundAtLocation(this, PedalSound, GetActorLocation());

    for (const FMagnetSlot& Slot : MagnetSlots)
    {
        AMagnet* Magnet = Slot.Magnet.Get();
        if (!Magnet) continue;

        Magnet->TogglePlatform();

        if (GEngine)
        {
            const TCHAR* Dir = Magnet->IsPlatformRaised() ? TEXT("올라감") : TEXT("내려감");
            const FColor Col = Magnet->IsPlatformRaised() ? FColor::Green : FColor::Red;
            GEngine->AddOnScreenDebugMessage(-1, 5.f, Col,
                FString::Printf(TEXT("[%s] %s"), *Magnet->GetName(), Dir));
        }
    }
}