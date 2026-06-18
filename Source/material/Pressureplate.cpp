#include "PressurePlate.h"
#include "Magnet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "UObject/ConstructorHelpers.h"   // ★ 추가
#include "Sound/SoundBase.h"              // ★ 추가
#include "Kismet/GameplayStatics.h" 

APressurePlate::APressurePlate()
{
    PrimaryActorTick.bCanEverTick = true;

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

    for (const FMagnetSlot& Slot : MagnetSlots)
    {
        AMagnet* Magnet = Slot.Magnet.Get();
        if (!Magnet) continue;

        const FVector Loc = Magnet->GetActorLocation();

if (Slot.bStartSunken)
{
    MagnetOriginalLocations.Add(Loc + FVector(0.f, 0.f, MoveDistance));
    MagnetTargetLocations.Add(Loc);
    MagnetMovedStates.Add(true);
    Magnet->ForceDemagnetize(); // ★ 이미 있음
}
        else
        {
            // ★ 처음에 올라가 있는 상태
            MagnetOriginalLocations.Add(Loc);
            MagnetTargetLocations.Add(Loc);
            MagnetMovedStates.Add(false);
        }
    }
}

void APressurePlate::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    for (int32 i = 0; i < MagnetSlots.Num(); ++i)
    {
        AMagnet* Magnet = MagnetSlots[i].Magnet.Get();
        if (!Magnet) continue;

        const FVector Current = Magnet->GetActorLocation();
        const FVector Target  = MagnetTargetLocations[i];

        if (FVector::Dist(Current, Target) < 1.f)
        {
            Magnet->SetActorLocation(Target);

            // 완전히 내려갔을 때 소자
            if (MagnetMovedStates[i] && !Magnet->IsDemagnetized())
                Magnet->ForceDemagnetize();

            continue;
        }

        const FVector New = FMath::VInterpConstantTo(Current, Target, DeltaTime, MoveSpeed);
        Magnet->SetActorLocation(New);
    }
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

    for (int32 i = 0; i < MagnetSlots.Num(); ++i)
    {
        AMagnet* Magnet = MagnetSlots[i].Magnet.Get();
        if (!Magnet) continue;

        if (MagnetMovedStates[i])
        {
            Magnet->Restore();
            MagnetTargetLocations[i] = MagnetOriginalLocations[i];
            MagnetMovedStates[i]     = false;
        }
        else
        {
            MagnetTargetLocations[i] = MagnetOriginalLocations[i]
                                     + FVector(0.f, 0.f, -MoveDistance);
            MagnetMovedStates[i]     = true;
        }
    }
} 