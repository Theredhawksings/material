#include "CoilGun.h"
#include "Generator.h"
#include "Wire.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
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
            ApplyMagneticForce();
        else
            ReleaseFire();
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

    // ── 열화상 스텐실: 코일은 전류 흐르면(철 유무 무관), 철은 흡입 중일 때 가열 ──
    const bool bCoilHeating = (CurrentVoltage > 0.f && bCurrentPositive);

    bool bIronHeating = false;
    if (bCoilHeating && CurrentState == ECoilGunState::Charging)
    {
        const float Current  = CurrentVoltage / FMath::Max(CoilResistance, 0.01f);
        const float Mu0      = 4.f * PI * 1e-7f;
        const float CoilR    = CoilRadiusCM / 100.f;
        const float CoilL    = FMath::Max(CoilLengthCM / 100.f, 0.01f);
        const float Area     = PI * CoilR * CoilR;
        const float ForceRaw = (FMath::Square((float)CoilWindings) * Mu0 * Area * FMath::Square(Current))
                             / (2.f * FMath::Square(CoilL));

        bIronHeating = (ForceRaw >= ThermalForceThreshold);
    }

    UpdateThermalStencil(DeltaTime, bCoilHeating, bIronHeating);
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
}

void ACoilGun::ReleaseFire()
{
    if (!LoadedIron || !LoadedIron->IsSimulatingPhysics()) return;

    LoadedIron->SetUseCCD(true);

    const FVector FireDir    = GetFireWorldDir();
    const FVector CurVel     = LoadedIron->GetPhysicsLinearVelocity();
    const float   ForwardSpd = FVector::DotProduct(CurVel, FireDir);

    const float LaunchSpd = FMath::Max(FMath::Abs(ForwardSpd), 50.f);
    LoadedIron->SetPhysicsLinearVelocity(FireDir * LaunchSpd);

    // ★ 발사 순간의 철 온도를 "날아가는 탄"에 넘김 → 잔열 띠고 식어가며 비행
    FiredIron        = LoadedIron;
    FiredIronStencil = IronStencil;
    IronStencil      = 0.f;

    UE_LOG(LogTemp, Warning, TEXT("CoilGun: 관성 발사! 속도=%.1f"), LaunchSpd);

    CurrentState = ECoilGunState::Fired;
}

void ACoilGun::UpdateThermalStencil(float DeltaTime, bool bCoilHeating, bool bIronHeating)
{
    if (!bEnableThermalStencil) return;

    auto Apply = [](UPrimitiveComponent* C, float V)
    {
        if (!C || !IsValid(C)) return;
        const bool bOn = (V > 0.5f);
        C->SetRenderCustomDepth(bOn);
        C->SetCustomDepthStencilValue(bOn ? FMath::RoundToInt(V) : 0);
    };

    // 1) 코일: 전류 흐르면 가열 (철 없어도 전원만 들어오면 달아오름)
    CoilStencil += (bCoilHeating ? ThermalHeatRate : -ThermalCoolRate) * DeltaTime;
    CoilStencil  = FMath::Clamp(CoilStencil, 0.f, 255.f);
    Apply(Cast<UPrimitiveComponent>(CoilMesh), CoilStencil);

    // 2) 장전된 철: 흡입 중 가열
    if (LoadedIron)
    {
        IronStencil += (bIronHeating ? ThermalHeatRate : -ThermalCoolRate) * DeltaTime;
        IronStencil  = FMath::Clamp(IronStencil, 0.f, 255.f);
        Apply(LoadedIron.Get(), IronStencil);
    }

    // 3) 발사된 철: 날아가며 천천히 냉각, 다 식으면 추적 해제
    if (FiredIron.IsValid())
    {
        FiredIronStencil -= ThermalCoolRate * DeltaTime;
        Apply(FiredIron.Get(), FiredIronStencil);   // 0.5 이하면 Apply가 알아서 끔
        if (FiredIronStencil <= 0.5f)
            FiredIron = nullptr;
    }
}