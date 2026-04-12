#include "AbsorptionCube.h"
#include "Components/StaticMeshComponent.h"

AAbsorptionCube::AAbsorptionCube()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    RootComponent = MeshComp;

    Tags.Add(TEXT("Absorption"));
}

void AAbsorptionCube::BeginPlay()
{
    Super::BeginPlay();
}

void AAbsorptionCube::ChargeAllGauges()
{
    // TODO: 캐릭터 게이지 충전 로직 추가
    Destroy();
}