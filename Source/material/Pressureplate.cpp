#include "PressurePlate.h"
#include "Magnet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"

APressurePlate::APressurePlate()
{
    PrimaryActorTick.bCanEverTick = false;

    PlateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlateMesh"));
    SetRootComponent(PlateMesh);
    PlateMesh->SetSimulatePhysics(false);

    DetectBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectBox"));
    DetectBox->SetupAttachment(PlateMesh);
    DetectBox->SetBoxExtent(FVector(60.f, 60.f, 20.f));
    DetectBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DetectBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    DetectBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void APressurePlate::BeginPlay()
{
    Super::BeginPlay();

    DetectBox->OnComponentBeginOverlap.AddDynamic(this, &APressurePlate::OnBoxBeginOverlap);
    DetectBox->OnComponentEndOverlap.AddDynamic(this,   &APressurePlate::OnBoxEndOverlap);
}

void APressurePlate::OnBoxBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    // 플레이어(캐릭터)만 감지
    if (!Cast<ACharacter>(OtherActor)) return;

    ++OverlapCount;

    if (OverlapCount == 1 && LinkedMagnet)
        LinkedMagnet->ForceDemagnetize();
}

void APressurePlate::OnBoxEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32)
{
    if (!Cast<ACharacter>(OtherActor)) return;

    OverlapCount = FMath::Max(OverlapCount - 1, 0);

    if (OverlapCount == 0 && LinkedMagnet)
        LinkedMagnet->Restore();
}