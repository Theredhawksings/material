#include "Resistance.h"
#include "Wire.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"

AResistance::AResistance()
{
    PrimaryActorTick.bCanEverTick = true;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    SetRootComponent(MeshComp);
    MeshComp->SetMobility(EComponentMobility::Movable);
    MeshComp->SetCollisionProfileName(TEXT("PhysicsActor"));
    MeshComp->SetGenerateOverlapEvents(true);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> BoxMesh(
        TEXT("/Game/modeling/Object/Box/Box1.Box1"));
    if (BoxMesh.Succeeded())
        MeshComp->SetStaticMesh(BoxMesh.Object);
}

void AResistance::BeginPlay()
{
    Super::BeginPlay();

    // Wire 의 END sphere 가 태그로 블럭을 감지 → "Metal" 태그 필요
    Tags.AddUnique(FName("Metal"));

    if (RefreshInterval > 0.f)
        GetWorldTimerManager().SetTimer(RefreshTimerHandle, this,
            &AResistance::RefreshConnectedWires, RefreshInterval, true);
}

void AResistance::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(RefreshTimerHandle);
    Super::EndPlay(EndPlayReason);
}

void AResistance::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

#if ENABLE_DRAW_DEBUG
    if (!bDrawDebug || !GetWorld()) return;

    const FVector Pos = GetActorLocation() + FVector(0.f, 0.f, 60.f);
    const FColor Col  = bElectrified ? FColor::Yellow : FColor::Silver;
    const int32  WireCount = ConnectedWires.Num();
    DrawDebugString(GetWorld(), Pos,
        FString::Printf(TEXT("[저항 %.1f Ohm] V:%.2f I:%.2fA  전선:%d"),
            ResistanceOhm, StoredVoltage, StoredCurrent, WireCount),
        nullptr, Col, 0.f, true);
#endif
}

void AResistance::RefreshConnectedWires()
{
    ConnectedWires.Reset();

    UWorld* World = GetWorld();
    if (!World || !MeshComp) return;

    // 블럭 중심에서 sphere 쿼리로 근처 전선 탐색 (Transformation_actor 와 동일)
    const FVector Center = MeshComp->Bounds.Origin;
    const float   Radius = FMath::Max(MeshComp->Bounds.SphereRadius + WireSenseExtraRadius, 5.f);

    FCollisionQueryParams Q(SCENE_QUERY_STAT(ResistanceWireSense), false);
    Q.AddIgnoredActor(this);

    TArray<FOverlapResult> Hits;
    World->OverlapMultiByObjectType(Hits, Center, FQuat::Identity,
        FCollisionObjectQueryParams::AllObjects,
        FCollisionShape::MakeSphere(Radius), Q);

    for (const FOverlapResult& H : Hits)
    {
        if (AWire* W = Cast<AWire>(H.GetActor()))
            ConnectedWires.AddUnique(W);
    }
}

void AResistance::ReceivePower(float InVoltage, float InCurrent)
{
    StoredVoltage = InVoltage;
    StoredCurrent = InCurrent;
    bElectrified  = (InVoltage > 0.f || InCurrent > 0.f);
    DebugVoltage  = StoredVoltage;
    DebugCurrent  = StoredCurrent;
}

void AResistance::ClearPower()
{
    StoredVoltage = 0.f;
    StoredCurrent = 0.f;
    bElectrified  = false;
    DebugVoltage  = 0.f;
    DebugCurrent  = 0.f;
}
