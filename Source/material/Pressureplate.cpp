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
    if (!Cast<ACharacter>(OtherActor)) return;

    const bool bWasEmpty = OverlappingPlayers.IsEmpty();
    OverlappingPlayers.Add(OtherActor);

    // 처음 진입할 때만 토글
    if (!bWasEmpty) return;
    if (!LinkedMagnet) return;

    // ★ 토글: 소자 상태면 복구, 아니면 소자
    if (LinkedMagnet->IsDemagnetized())
        LinkedMagnet->Restore();
    else
        LinkedMagnet->ForceDemagnetize();
}

void APressurePlate::OnBoxEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32)
{
    if (!Cast<ACharacter>(OtherActor)) return;
    OverlappingPlayers.Remove(OtherActor);\
    // 나갈 때는 아무것도 안 함
}