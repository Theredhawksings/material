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
    // 철이 코일 메시에 부딪히지 않게 NoCollision
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

    switch (CurrentState)
    {
    case ECoilGunState::Idle:
        // Wire에서 전압 읽기
        ReadVoltageFromWires();
        // 전압 있으면 철 감지 후 발사
        if (CurrentVoltage > 0.f)
            DetectAndFire();
        break;

    case ECoilGunState::Fire:
        // 발사는 DoFire에서 즉시 처리
        // 바로 Cooldown으로
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

FVector ACoilGun::GetFireWorldDir() const
{
    return GetActorTransform()
        .TransformVectorNoScale(FireDirection)
        .GetSafeNormal();
}

void ACoilGun::ReadVoltageFromWires()
{
    CurrentVoltage = 0.f;

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

        // Wire 끝점이 CoilGun 근처인지 확인
        const float EndDist = FVector::Dist(
            GetActorLocation(), Wire->GetEndPointLocation());
        if (EndDist > WireDetectRadius) continue;

        // 가장 높은 전압 사용
        CurrentVoltage = FMath::Max(CurrentVoltage, Wire->GetEffectiveVoltage());
        ConnectedWires.Add(Wire);
    }

    // ★ 테스트용: 전압 없으면 기본값 사용
    if (CurrentVoltage <= 0.f)
        CurrentVoltage = DefaultLaunchSpeed / VoltageToSpeedMultiplier;
}

void ACoilGun::DetectAndFire()
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

        // 철 발견 → 즉시 발사
        DoFire(Comp);
        CurrentState = ECoilGunState::Fire;
        break;
    }
}

void ACoilGun::DoFire(UPrimitiveComponent* IronComp)
{
    if (!IronComp || !IronComp->IsSimulatingPhysics()) return;

    const FVector FireDir = GetFireWorldDir();

    // 전압 → 발사 속도 계산
    float LaunchSpeed = CurrentVoltage * VoltageToSpeedMultiplier;
    LaunchSpeed = FMath::Clamp(LaunchSpeed, MinLaunchSpeed, MaxLaunchSpeed);

    // 발사 방향으로만 속도 설정
    IronComp->SetPhysicsLinearVelocity(FireDir * LaunchSpeed);
    IronComp->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

    UE_LOG(LogTemp, Log, TEXT("CoilGun: 발사! 전압=%.1fV 속도=%.1f"),
        CurrentVoltage, LaunchSpeed);

#if ENABLE_DRAW_DEBUG
    // 발사 궤적 3초간 표시
    DrawDebugLine(GetWorld(),
        GetActorLocation(),
        GetActorLocation() + FireDir * 3000.f,
        FColor::Red, false, 3.f, 0, 5.f);

    DrawDebugSphere(GetWorld(),
        IronComp->GetComponentLocation(),
        20.f, 8, FColor::Yellow, false, 3.f);
#endif
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
        StateColor = CurrentVoltage > 0.f ? FColor::Green : FColor::White;
        StateStr   = TEXT("IDLE");
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
        MyLoc, MyLoc + FireDir * 300.f,
        30.f, StateColor, false, -1.f, 0, 3.f);

    // Wire 감지 범위
    DrawDebugSphere(GetWorld(), MyLoc,
        WireDetectRadius, 12, FColor::Purple, false, -1.f, 0, 1.f);

    // 상태 텍스트
    DrawDebugString(GetWorld(), MyLoc + FVector(0, 0, 80.f),
        FString::Printf(TEXT(
            "[CoilGun]\n"
            "상태: %s\n"
            "전압: %.1f V\n"
            "발사속도: %.0f\n"
            "연결Wire: %d개"
        ),
            *StateStr,
            CurrentVoltage,
            FMath::Clamp(CurrentVoltage * VoltageToSpeedMultiplier,
                MinLaunchSpeed, MaxLaunchSpeed),
            ConnectedWires.Num()
        ),
        nullptr, StateColor, 0.f, true);
#endif
}