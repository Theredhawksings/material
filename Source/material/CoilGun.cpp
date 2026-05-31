#include "CoilGun.h"
#include "Generator.h"
#include "Wire.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"

ACoilGun::ACoilGun()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    CoilMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoilMesh"));
    CoilMesh->SetupAttachment(Root);
    CoilMesh->SetSimulatePhysics(false);
    CoilMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    BarrelZone = CreateDefaultSubobject<UBoxComponent>(TEXT("BarrelZone"));
    BarrelZone->SetupAttachment(Root);
    BarrelZone->SetBoxExtent(FVector(30.f, 80.f, 30.f));
    BarrelZone->SetRelativeLocation(FVector::ZeroVector);
    BarrelZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BarrelZone->SetCollisionResponseToAllChannels(ECR_Overlap);
}

void ACoilGun::BeginPlay()
{
    Super::BeginPlay();
}

void ACoilGun::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    ReadWireState();

    switch (CurrentState)
    {
    case ECoilGunState::Idle:
        // 전압 있으면 무조건 철 감지
        if (CurrentVoltage > 0.f)
            DetectIron();
        break;

    case ECoilGunState::Charging:
        if (!LoadedIron || !IsValid(LoadedIron.Get()))
        {
            CurrentState = ECoilGunState::Idle;
            break;
        }

        if (bCurrentPositive)
        {
            // sin > 0 → 인력
            ApplyMagneticForce();
        }
        else
        {
            // sin < 0 → 전원 차단 → 관성 발사
            ReleaseFire();
        }
        break;

    case ECoilGunState::Fired:
        CurrentState  = ECoilGunState::Cooldown;
        CooldownTimer = 0.f;
        LoadedIron    = nullptr;
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

FVector ACoilGun::GetFireWorldDir() const
{
    return GetActorTransform()
        .TransformVectorNoScale(FireDirection)
        .GetSafeNormal();
}

void ACoilGun::ReadWireState()
{
    CurrentVoltage   = 0.f;
    bCurrentPositive = false;

    // ★ Generator 직접 참조 (가장 정확한 방법)
    if (ConnectedGenerator)
    {
        const float EMF  = ConnectedGenerator->GetCurrentEMF();
        CurrentVoltage   = FMath::Abs(EMF);
        bCurrentPositive = ConnectedGenerator->IsCurrentPositive();
        return;
    }

    // Generator 없으면 Wire 태그로 fallback
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
        if (!Wire || !Wire->IsPowered()) continue;

        const float EndDist = FVector::Dist(
            GetActorLocation(), Wire->GetEndPointLocation());
        if (EndDist > WireDetectRadius) continue;

        CurrentVoltage = FMath::Max(CurrentVoltage, Wire->GetEffectiveVoltage());

        if (Wire->ActorHasTag(FName("CurrentPositive")))
            bCurrentPositive = true;
        else if (Wire->ActorHasTag(FName("CurrentNegative")))
            bCurrentPositive = false;

        ConnectedWires.Add(Wire);
    }
}

void ACoilGun::DetectIron()
{
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

        LoadedIron   = Comp;
        CurrentState = ECoilGunState::Charging;
        UE_LOG(LogTemp, Log, TEXT("CoilGun: 철 감지 → 흡입 시작"));
        break;
    }
}

void ACoilGun::ApplyMagneticForce()
{
    if (!LoadedIron || !LoadedIron->IsSimulatingPhysics()) return;

    // ── 실제 코일건 물리 공식 ──
    // F = (N² × μ₀ × A × I²) / (2 × L²)
    const float Mu0        = 4.f * PI * 1e-7f;
    const float CoilRadius = CoilRadiusCM / 100.f;
    const float CoilLength = FMath::Max(CoilLengthCM / 100.f, 0.01f);
    const float Area       = PI * CoilRadius * CoilRadius;
    const float Current    = CurrentVoltage / FMath::Max(CoilResistance, 0.01f);

    float ForceMag = (FMath::Square((float)CoilWindings) * Mu0 * Area * FMath::Square(Current))
                   / (2.f * FMath::Square(CoilLength));

    ForceMag *= ForceScaleMultiplier;

    const FVector CoilCenter = GetActorLocation();
    const FVector IronLoc    = LoadedIron->GetComponentLocation();
    const FVector ToCenter   = CoilCenter - IronLoc;
    const float   Distance   = ToCenter.Size();

    if (Distance < 1.f) return;

    // 코일 축 방향으로만 힘 적용
    const FVector FireDir   = GetFireWorldDir();
    const FVector AxisForce = FireDir * FMath::Abs(
        FVector::DotProduct(ToCenter.GetSafeNormal(), FireDir)) * ForceMag;

    // 코일 축 방향 속도만 유지 (옆으로 튀는 거 방지)
    const FVector CurVel    = LoadedIron->GetPhysicsLinearVelocity();
    const float   AxisSpeed = FVector::DotProduct(CurVel, -FireDir);
    const FVector AxisVel   = -FireDir * AxisSpeed;
    LoadedIron->SetPhysicsLinearVelocity(AxisVel);

    // 최대 속도 제한
    if (AxisSpeed < MaxPullSpeed)
        LoadedIron->AddForce(AxisForce, NAME_None, true);

    // 회전 고정
    LoadedIron->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

#if ENABLE_DRAW_DEBUG
    if (bDebugDraw)
    {
        DrawDebugDirectionalArrow(GetWorld(),
            IronLoc, IronLoc + AxisForce.GetSafeNormal() * 80.f,
            20.f, FColor::Cyan, false, -1.f, 0, 3.f);

        DrawDebugString(GetWorld(), IronLoc + FVector(0, 0, 50.f),
            FString::Printf(TEXT("I: %.1fA\nF: %.1fN"),
                Current, ForceMag / ForceScaleMultiplier),
            nullptr, FColor::Cyan, 0.f, true);
    }
#endif
}

void ACoilGun::ReleaseFire()
{
    if (!LoadedIron || !LoadedIron->IsSimulatingPhysics()) return;

    const FVector FireDir    = GetFireWorldDir();
    const FVector CurVel     = LoadedIron->GetPhysicsLinearVelocity();
    const float   ForwardSpd = FVector::DotProduct(CurVel, FireDir);

    // ★ 최소 속도 보장
    const float LaunchSpd = FMath::Max(FMath::Abs(ForwardSpd), 50.f);
    LoadedIron->SetPhysicsLinearVelocity(FireDir * LaunchSpd);

    UE_LOG(LogTemp, Warning, TEXT("CoilGun: 관성 발사! 속도=%.1f"), LaunchSpd);

#if ENABLE_DRAW_DEBUG
    DrawDebugLine(GetWorld(),
        GetActorLocation(),
        GetActorLocation() + FireDir * 3000.f,
        FColor::Red, false, 3.f, 0, 5.f);
#endif

    CurrentState = ECoilGunState::Fired;
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
        StateStr   = TEXT("CHARGING (흡입중)");
        break;
    case ECoilGunState::Fired:
        StateColor = FColor::Yellow;
        StateStr   = TEXT("FIRED!");
        break;
    case ECoilGunState::Cooldown:
        StateColor = FColor::Orange;
        StateStr   = FString::Printf(TEXT("COOLDOWN (%.1f)"),
            CooldownTime - CooldownTimer);
        break;
    }

    DrawDebugBox(GetWorld(), MyLoc,
        BarrelZone->GetScaledBoxExtent(),
        GetActorQuat(), StateColor, false, -1.f, 0, 2.f);

    DrawDebugDirectionalArrow(GetWorld(),
        MyLoc, MyLoc + FireDir * 300.f,
        30.f, StateColor, false, -1.f, 0, 3.f);

    DrawDebugSphere(GetWorld(), MyLoc,
        WireDetectRadius, 12, FColor::Purple, false, -1.f, 0, 1.f);

    const float Current  = CurrentVoltage / FMath::Max(CoilResistance, 0.01f);
    const float CoilR    = CoilRadiusCM / 100.f;
    const float CoilL    = FMath::Max(CoilLengthCM / 100.f, 0.01f);
    const float Area     = PI * CoilR * CoilR;
    const float Mu0      = 4.f * PI * 1e-7f;
    const float ForceRaw = (FMath::Square((float)CoilWindings) * Mu0 * Area * FMath::Square(Current))
                         / (2.f * FMath::Square(CoilL));

    DrawDebugString(GetWorld(), MyLoc + FVector(0, 0, 80.f),
        FString::Printf(TEXT(
            "[CoilGun]\n"
            "상태: %s\n"
            "전압: %.1fV\n"
            "전류: %.1fA\n"
            "이론 힘: %.4fN\n"
            "전류방향: %s\n"
            "철: %s\n"
            "연결Wire: %d개\n"
            "Generator: %s"
        ),
            *StateStr,
            CurrentVoltage,
            Current,
            ForceRaw,
            bCurrentPositive ? TEXT("정방향(+)") : TEXT("역방향(-)"),
            LoadedIron ? TEXT("장전됨") : TEXT("없음"),
            ConnectedWires.Num(),
            ConnectedGenerator ? TEXT("연결됨 ✅") : TEXT("없음 ❌")
        ),
        nullptr, StateColor, 0.f, true);
#endif
}