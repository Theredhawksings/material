#include "Resistance.h"
#include "Wire.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInterface.h"
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

    static ConstructorHelpers::FObjectFinder<UStaticMesh> ResistorMesh(
        TEXT("/Game/modeling/Object/newBlock/Rasistance_Block.Rasistance_Block"));
    if (ResistorMesh.Succeeded())
        MeshComp->SetStaticMesh(ResistorMesh.Object);

    MeshComp->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.6f));

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ResistorMat(
        TEXT("/Game/modeling/Object/newBlock/M_Rasistance.M_Rasistance"));
    if (ResistorMat.Succeeded())
        MeshComp->SetMaterial(0, ResistorMat.Object);

    // 옴 값 표시 텍스트
    OhmText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("OhmText"));
    OhmText->SetupAttachment(MeshComp);
    OhmText->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
    OhmText->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
    OhmText->SetHorizontalAlignment(EHTA_Center);
    OhmText->SetVerticalAlignment(EVRTA_TextCenter);
    OhmText->SetWorldSize(80.f);
    OhmText->SetTextRenderColor(FColor::White);
}

void AResistance::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // 에디터에서 ResistanceOhm 값을 바꾸면 텍스트도 즉시 갱신
    if (OhmText)
        OhmText->SetText(FText::FromString(
            FString::Printf(TEXT("%g"), ResistanceOhm)));
}

void AResistance::BeginPlay()
{
    Super::BeginPlay();

    // Wire 의 END sphere 가 태그로 블럭을 감지 → "Metal" 태그 필요
    Tags.AddUnique(FName("Metal"));

    ApplyPhysicsSettings();

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

void AResistance::ApplyPhysicsSettings()
{
    if (!MeshComp) return;

    if (bFixedInPlace)
    {
        // 완전 고정: 물리 끄고 충돌만 유지
        MeshComp->SetSimulatePhysics(false);
        MeshComp->SetEnableGravity(false);
        MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        return;
    }

    // 무겁게 + 금방 멈추게 → 플레이어가 지나가며 밀어도 회로에서 안 빠짐
    MeshComp->SetMassOverrideInKg(NAME_None, FMath::Max(MassKg, 1.f), true);
    MeshComp->SetLinearDamping(LinearDamping);
    MeshComp->SetAngularDamping(AngularDamping);
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
