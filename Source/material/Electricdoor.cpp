#include "ElectricDoor.h"
#include "Wire.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/OverlapResult.h"

AElectricDoor::AElectricDoor()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // 전선 끝을 갖다 대는 박스 - 에디터에서 위치 조절 가능
    WireContactBox = CreateDefaultSubobject<UBoxComponent>(TEXT("WireContactBox"));
    WireContactBox->SetupAttachment(Root);
    WireContactBox->SetBoxExtent(FVector(40.f, 40.f, 40.f));
    WireContactBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WireContactBox->SetCollisionResponseToAllChannels(ECR_Ignore);
}

void AElectricDoor::BeginPlay()
{
    Super::BeginPlay();

    if (!DoorActor) return;

    // 큐브 액터의 시작 위치 저장
    ClosedLocation = DoorActor->GetActorLocation();
    OpenLocation   = ClosedLocation + OpenDirection.GetSafeNormal() * OpenDistance;
}

void AElectricDoor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bOpened || !DoorActor) return;

    CheckWirePower();

    // 열리는 중이면 큐브 액터 위치 보간
    if (bOpening)
    {
        const FVector Current = DoorActor->GetActorLocation();
        const FVector New     = FMath::VInterpConstantTo(
            Current, OpenLocation, DeltaTime, OpenSpeed);

        DoorActor->SetActorLocation(New);

        // 목표 위치 도달하면 완전히 열림
        if (FVector::Dist(New, OpenLocation) < 1.f)
        {
            DoorActor->SetActorLocation(OpenLocation);
            bOpened  = true;
            bOpening = false;
        }
    }
}

void AElectricDoor::CheckWirePower()
{
    if (bOpening || bOpened) return;

    const FVector BoxCenter = WireContactBox->GetComponentLocation();
    const FQuat   BoxRot    = WireContactBox->GetComponentQuat();
    const FVector BoxExtent = WireContactBox->GetScaledBoxExtent();

    TArray<FOverlapResult> Hits;
    FCollisionQueryParams QParams(SCENE_QUERY_STAT(DoorWireSense), false);
    QParams.AddIgnoredActor(this);

    GetWorld()->OverlapMultiByObjectType(
        Hits, BoxCenter, BoxRot,
        FCollisionObjectQueryParams::AllObjects,
        FCollisionShape::MakeBox(BoxExtent), QParams);

    for (const FOverlapResult& H : Hits)
    {
        AWire* Wire = Cast<AWire>(H.GetActor());
        if (!Wire) continue;

        // 전선 끝점이 박스 안에 있는지 확인
        const FVector EndPt   = Wire->GetEndPointLocation();
        const FVector LocalPt = WireContactBox->GetComponentTransform()
                                               .InverseTransformPosition(EndPt);

        if (FMath::Abs(LocalPt.X) > BoxExtent.X ||
            FMath::Abs(LocalPt.Y) > BoxExtent.Y ||
            FMath::Abs(LocalPt.Z) > BoxExtent.Z) continue;

        // 전선에 전기가 들어오면 문 열기
        if (Wire->IsPowered() && Wire->GetEffectiveVoltage() > 0.f)
        {
            OpenDoor();
            return;
        }
    }
}	

void AElectricDoor::OpenDoor()
{
    if (bOpening || bOpened) return;
    bOpening = true;
}