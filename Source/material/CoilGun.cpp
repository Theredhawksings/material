#include "CoilGun.h"
#include "Wire.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"

ACoilGun::ACoilGun()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    CoilMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoilMesh"));
    CoilMesh->SetupAttachment(Root);
    CoilMesh->SetSimulatePhysics(false);
    CoilMesh->SetCollisionProfileName(TEXT("BlockAll"));

    BarrelZone = CreateDefaultSubobject<UBoxComponent>(TEXT("BarrelZone"));
    BarrelZone->SetupAttachment(Root);
    BarrelZone->SetBoxExtent(FVector(30.f, 80.f, 30.f)); // Y축으로 길게 (기본 -Y 발사)
    BarrelZone->SetRelativeLocation(FVector::ZeroVector);
    BarrelZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BarrelZone->SetCollisionResponseToAllChannels(ECR_Overlap);
}

void ACoilGun::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        EnableInput(PC);
        if (InputComponent)
        {
            InputComponent->BindKey(EKeys::F, IE_Pressed,
                this, &ACoilGun::OnTriggerPressed);
            InputComponent->BindKey(EKeys::F, IE_Released,
                this, &ACoilGun::OnTriggerReleased);
        }
    }
}

void ACoilGun::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    switch (CurrentState)
    {
    case ECoilGunState::Idle:
        LoadedIron = nullptr;
        PowerRatio = 0.f;
        break;

    case ECoilGunState::Charging:
        DetectIron();
        if (LoadedIron)
        {
            ApplyPullForce();
            CheckIronReachedCenter();
            UpdatePowerRatio();
        }
        break;

    case ECoilGunState::Fire:
        DoFire();
        CurrentState  = ECoilGunState::Cooldown;
        CooldownTimer = 0.f;
        break;

    case ECoilGunState::Cooldown:
        CooldownTimer += DeltaTime;
        if (CooldownTimer >= CooldownTime)
        {
            CurrentState = ECoilGunState::Idle;
            UE_LOG(LogTemp, Log, TEXT("CoilGun: 재장전 완료"));
        }
        break;
    }

    DebugVisualize();
}

// 월드 기준 발사 방향 반환
FVector ACoilGun::GetFireWorldDir() const
{
    return GetActorTransform()
        .TransformVectorNoScale(FireDirection)
        .GetSafeNormal();
}

// F 누름 → 전원 ON → 철 흡입 시작
void ACoilGun::OnTriggerPressed()
{
    if (CurrentState != ECoilGunState::Idle) return;

    CurrentState = ECoilGunState::Charging;
    UpdateWireConnection(true);
    UE_LOG(LogTemp, Log, TEXT("CoilGun: 전원 ON - 흡입 시작"));
}

// F 놓음 → 전원 OFF → 발사
void ACoilGun::OnTriggerReleased()
{
    if (CurrentState != ECoilGunState::Charging) return;

    UpdateWireConnection(false);

    if (LoadedIron)
    {
        CurrentState = ECoilGunState::Fire;
        UE_LOG(LogTemp, Log, TEXT("CoilGun: 전원 OFF - 발사!"));
    }
    else
    {
        CurrentState = ECoilGunState::Idle;
        UE_LOG(LogTemp, Log, TEXT("CoilGun: 전원 OFF - 철 없음"));
    }
}

void ACoilGun::DetectIron()
{
    if (LoadedIron) return;

    TArray<FOverlapResult> Hits;
    FCollisionQueryParams QParams(SCENE_QUERY_STAT(CoilGunDetect), false);
    QParams.AddIgnoredActor(this);

    GetWorld()->OverlapMultiByObjectType(
        Hits, GetActorLocation(), GetActorQuat(),
        FCollisionObjectQueryParams::AllObjects,
        FCollisionShape::MakeBox(BarrelZone->GetScaledBoxExtent()),
        QParams);

    for (const FOverlapResult& H : Hits)
    {
        AActor* HitActor = H.GetActor();
        if (!HitActor || !HitActor->ActorHasTag(IronTag)) continue;

        UPrimitiveComponent* Comp = H.GetComponent();
        if (!Comp || !Comp->IsSimulatingPhysics()) continue;

        LoadedIron = Comp;
        UE_LOG(LogTemp, Log, TEXT("CoilGun: 철 감지 - %s"),
            *HitActor->GetName());
        break;
    }
}

void ACoilGun::ApplyPullForce()
{
    if (!LoadedIron || !LoadedIron->IsSimulatingPhysics()) return;

    const FVector CoilCenter = GetActorLocation();
    const FVector IronLoc    = LoadedIron->GetComponentLocation();
    const FVector PullDir    = (CoilCenter - IronLoc).GetSafeNormal();
    const float   Distance   = FVector::Dist(CoilCenter, IronLoc);

    // 가까울수록 강한 인력
    const float DistFactor = FMath::Clamp(1.f - (Distance / 500.f), 0.1f, 1.f);
    LoadedIron->AddForce(PullDir * PullForce * DistFactor, NAME_None, true);
}

void ACoilGun::CheckIronReachedCenter()
{
    if (!LoadedIron) return;

    const float Dist = FVector::Dist(
        GetActorLocation(), LoadedIron->GetComponentLocation());

    if (Dist < CenterThreshold)
    {
        // 발사 방향 속도만 남기고 나머지 제거
        const FVector FireDir  = GetFireWorldDir();
        const FVector CurVel   = LoadedIron->GetPhysicsLinearVelocity();
        const float   ForwardSpd = FVector::DotProduct(CurVel, FireDir);
        LoadedIron->SetPhysicsLinearVelocity(FireDir * FMath::Max(ForwardSpd, 0.f));
    }
}

void ACoilGun::DoFire()
{
    if (!LoadedIron || !LoadedIron->IsSimulatingPhysics()) return;

    const FVector FireDir  = GetFireWorldDir();
    const FVector CurVel   = LoadedIron->GetPhysicsLinearVelocity();
    const float   CurSpd   = FVector::DotProduct(CurVel, FireDir);
    const float   LaunchSpd = FMath::Max(FMath::Abs(CurSpd), MinLaunchSpeed);

    LoadedIron->SetPhysicsLinearVelocity(FireDir * LaunchSpd);

    UE_LOG(LogTemp, Log, TEXT("CoilGun: 발사 방향=%s 속도=%.1f"),
        *FireDir.ToString(), LaunchSpd);

    LoadedIron = nullptr;
    PowerRatio = 0.f;
}

void ACoilGun::UpdatePowerRatio()
{
    if (!LoadedIron) { PowerRatio = 0.f; return; }

    const float Dist    = FVector::Dist(
        GetActorLocation(), LoadedIron->GetComponentLocation());
    const float MaxDist = 500.f;

    PowerRatio = FMath::Clamp(1.f - (Dist / MaxDist), 0.f, 1.f);
}

void ACoilGun::UpdateWireConnection(bool bPowered)
{
    TArray<FOverlapResult> Hits;
    FCollisionQueryParams QParams(SCENE_QUERY_STAT(CoilGunWire), false);
    QParams.AddIgnoredActor(this);

    GetWorld()->OverlapMultiByObjectType(
        Hits, GetActorLocation(), FQuat::Identity,
        FCollisionObjectQueryParams::AllObjects,
        FCollisionShape::MakeSphere(WireDetectRadius), QParams);

    ConnectedWires.Empty();

    for (const FOverlapResult& H : Hits)
    {
        AWire* Wire = Cast<AWire>(H.GetActor());
        if (!Wire) continue;

        const float EndDist = FVector::Dist(
            GetActorLocation(), Wire->GetEndPointLocation());
        if (EndDist > WireDetectRadius) continue;

        ConnectedWires.Add(Wire);
    }
}

void ACoilGun::DebugVisualize()
{
#if ENABLE_DRAW_DEBUG
    if (!bDebugDraw) return;

    const FVector MyLoc   = GetActorLocation();
    const FVector FireDir = GetFireWorldDir();

    FColor  StateColor = FColor::White;
    FString StateStr   = TEXT("IDLE");

    switch (CurrentState)
    {
    case ECoilGunState::Idle:
        StateColor = FColor::White;
        StateStr   = TEXT("IDLE");
        break;
    case ECoilGunState::Charging:
        StateColor = FColor::Green;
        StateStr   = TEXT("CHARGING (F 놓으면 발사!)");
        break;
    case ECoilGunState::Fire:
        StateColor = FColor::Yellow;
        StateStr   = TEXT("FIRE!");
        break;
    case ECoilGunState::Cooldown:
        StateColor = FColor::Orange;
        StateStr   = FString::Printf(TEXT("COOLDOWN (%.1f)"),
            CooldownTime - CooldownTimer);
        break;
    }

    // 배럴 존
    DrawDebugBox(GetWorld(), MyLoc,
        BarrelZone->GetScaledBoxExtent(),
        GetActorQuat(), StateColor, false, -1.f, 0, 2.f);

    // 발사 방향 화살표
    DrawDebugDirectionalArrow(GetWorld(),
        MyLoc, MyLoc + FireDir * 200.f,
        30.f, StateColor, false, -1.f, 0, 3.f);

    // 상태 텍스트
    DrawDebugString(GetWorld(), MyLoc + FVector(0, 0, 80.f),
        FString::Printf(TEXT(
            "[CoilGun]\n"
            "상태: %s\n"
            "발사력: %.0f%%\n"
            "철: %s"
        ),
            *StateStr,
            PowerRatio * 100.f,
            LoadedIron ? TEXT("장전됨") : TEXT("없음")
        ),
        nullptr, StateColor, 0.f, true);

    // 철과 연결선
    if (LoadedIron)
    {
        DrawDebugLine(GetWorld(),
            MyLoc, LoadedIron->GetComponentLocation(),
            FColor::Cyan, false, -1.f, 0, 2.f);
    }

    // Wire 감지 범위
    DrawDebugSphere(GetWorld(), MyLoc,
        WireDetectRadius, 12, FColor::Purple, false, -1.f, 0, 1.f);
#endif
}