#include "Generator.h"
#include "Magnet.h"
#include "Wire.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "UObject/ConstructorHelpers.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

AGenerator::AGenerator()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    GeneratorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GeneratorMesh"));
    GeneratorMesh->SetupAttachment(Root);
    GeneratorMesh->SetSimulatePhysics(false);
    GeneratorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    OutputBox = CreateDefaultSubobject<UBoxComponent>(TEXT("OutputBox"));
    OutputBox->SetupAttachment(Root);
    OutputBox->SetBoxExtent(FVector(40.f, 40.f, 40.f));
    OutputBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OutputBox->SetCollisionResponseToAllChannels(ECR_Ignore);

    static ConstructorHelpers::FObjectFinder<USoundBase> TurnOnAsset(
        TEXT("/Script/Engine.SoundWave'/Game/Sound/sound_generator_turning_on.sound_generator_turning_on'"));
    if (TurnOnAsset.Succeeded())
        TurningOnSound = TurnOnAsset.Object;

    static ConstructorHelpers::FObjectFinder<USoundBase> TurnOffAsset(
        TEXT("/Script/Engine.SoundWave'/Game/Sound/sound_generator_turning_off.sound_generator_turning_off'"));
    if (TurnOffAsset.Succeeded())
        TurningOffSound = TurnOffAsset.Object;
}

void AGenerator::BeginPlay()
{
    Super::BeginPlay();
    DetectMagnets();
}

void AGenerator::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ── 자석 스캔은 주기적으로만 (EMF/회로는 매 프레임 그대로) ──
    // GetAllActorsOfClass는 월드 전체 액터를 순회하는 비싼 호출이라
    // 매 프레임이 아니라 일정 주기로만 갱신한다. (MagnetScanInterval = 0 이면 매 프레임)
    MagnetScanAccumulator += DeltaTime;
    if (MagnetScanInterval <= 0.f || MagnetScanAccumulator >= MagnetScanInterval)
    {
        MagnetScanAccumulator = 0.f;
        DetectMagnets();
    }

    UpdateEMF(DeltaTime);
    UpdateCircuit();
    UpdateGeneratorSound();

#if ENABLE_DRAW_DEBUG
    if (!bDebugDraw) return;

    const FVector MyLoc    = GetActorLocation();
    const FVector BoxLoc   = OutputBox->GetComponentLocation();
    const bool bGenerating = (FMath::Abs(CurrentEMF) >= MinEMFThreshold);
    const bool bOutputOn   = bCoilOutputEnabled && bGenerating && bCurrentPositive;

    DrawDebugSphere(GetWorld(), MyLoc,
        MagnetDetectRadius, 16, FColor::Yellow, false, -1.f, 0, 1.f);

    DrawDebugBox(GetWorld(), BoxLoc,
        OutputBox->GetScaledBoxExtent(), OutputBox->GetComponentQuat(),
        bOutputOn ? FColor::Cyan : FColor(100, 100, 100),
        false, -1.f, 0, 2.f);

    for (AMagnet* M : NorthMagnets)
        if (M) DrawDebugSphere(GetWorld(), M->GetActorLocation(),
            40.f, 8, FColor::Red, false, -1.f, 0, 3.f);
    for (AMagnet* M : SouthMagnets)
        if (M) DrawDebugSphere(GetWorld(), M->GetActorLocation(),
            40.f, 8, FColor::Blue, false, -1.f, 0, 3.f);

    DrawDebugString(GetWorld(), MyLoc + FVector(0, 0, 100.f),
        FString::Printf(TEXT(
            "[Generator]\n"
            "N극: %d개 / S극: %d개\n"
            "유효쌍: %d / 불균형: %.0f%%\n"
            "회전속도: %.1f deg/s\n"
            "회전각: %.1f\n"
            "EMF: %.2f V\n"
            "전류방향: %s\n"
            "전기출력: %s\n"
            "박스내Wire: %d개"),
            NorthMagnets.Num(), SouthMagnets.Num(),
            EffectivePairs, ImbalanceRatio * 100.f,
            CurrentRotationSpeed,
            RotationAngle,
            CurrentEMF,
            bCurrentPositive ? TEXT("정방향(+)") : TEXT("역방향(-)"),
            bOutputOn ? TEXT("ON") : TEXT("OFF"),
            BoxPoweredWires.Num()),
        nullptr, FColor::Green, 0.f, true);
#endif
}

void AGenerator::DetectMagnets()
{
    NorthMagnets.Reset();   // Empty() 대신 Reset() → 내부 용량 유지, 재할당 감소
    SouthMagnets.Reset();

    const FVector MyLoc    = GetActorLocation();
    const float   RadiusSq = FMath::Square(MagnetDetectRadius);

    TArray<AActor*> FoundMagnets;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMagnet::StaticClass(), FoundMagnets);

    for (AActor* Actor : FoundMagnets)
    {
        AMagnet* Magnet = Cast<AMagnet>(Actor);
        if (!Magnet || Magnet->IsDemagnetized()) continue;

        // sqrt 없는 제곱거리 비교 (결과는 동일)
        if (FVector::DistSquared(MyLoc, Magnet->GetActorLocation()) > RadiusSq) continue;

        if (Magnet->IsNorthPole()) NorthMagnets.Add(Magnet);
        else                       SouthMagnets.Add(Magnet);
    }
}

void AGenerator::UpdateEMF(float DeltaTime)
{
    if (!bGeneratorActive || (NorthMagnets.Num() == 0 && SouthMagnets.Num() == 0))
    {
        CurrentEMF           = 0.f;
        RotationAngle        = 0.f;
        EffectivePairs       = 0;
        ImbalanceRatio       = 0.f;
        CurrentRotationSpeed = BaseRotationSpeed;
        GeneratorMesh->SetRelativeRotation(FRotator::ZeroRotator);
        return;
    }

    const FVector MyLoc = GetActorLocation();   // 한 번만 캐시

    // ── 1) 유효 쌍 수 & 불균형도 ────────────────────────────────
    const int32 NCount = NorthMagnets.Num();
    const int32 SCount = SouthMagnets.Num();
    EffectivePairs = FMath::Min(NCount, SCount);

    ImbalanceRatio = (NCount + SCount > 0)
        ? (float)FMath::Abs(NCount - SCount) / (float)(NCount + SCount)
        : 0.f;

    if (EffectivePairs == 0)
    {
        CurrentEMF = 0.f;
        return;
    }

    // ── 2) 회전속도: 쌍 많을수록 느려짐 ────────────────────────
    CurrentRotationSpeed = BaseRotationSpeed / (float)EffectivePairs;

    // ── 3) 회전 방향: N극 무게중심 기준 ────────────────────────
    FVector NorthCentroid = FVector::ZeroVector;
    for (AMagnet* M : NorthMagnets)
        if (M) NorthCentroid += M->GetActorLocation();
    NorthCentroid /= (float)NorthMagnets.Num();

    const FVector ToNorth = (NorthCentroid - MyLoc).GetSafeNormal();
    const float Dot    = FVector::DotProduct(ToNorth, GetActorRightVector());
    const float RotDir = (Dot > 0.f) ? 1.f : -1.f;

    RotationAngle += RotDir * CurrentRotationSpeed * DeltaTime;
    if (RotationAngle >= 360.f)  RotationAngle -= 360.f;
    if (RotationAngle <= -360.f) RotationAngle += 360.f;

    GeneratorMesh->SetRelativeRotation((SpinAxisMask * RotationAngle).Quaternion());

    // ── 4) B(자기장): 거리 한 번만 계산해서 정렬 + 쌍 합산 ──────
    // 기존엔 Sort 비교마다 GetActorLocation() + Dist(sqrt)를 두 번씩 호출했음.
    // 거리(제곱)를 미리 한 번만 계산해 정렬 비용을 줄인다.
    struct FMagnetDistSq { AMagnet* Magnet; float DistSq; };
    TArray<FMagnetDistSq, TInlineAllocator<8>> SortedN, SortedS;
    SortedN.Reserve(NCount);
    SortedS.Reserve(SCount);

    for (const TObjectPtr<AMagnet>& M : NorthMagnets)
        if (M) SortedN.Add({ M.Get(), (float)FVector::DistSquared(MyLoc, M->GetActorLocation()) });
    for (const TObjectPtr<AMagnet>& M : SouthMagnets)
        if (M) SortedS.Add({ M.Get(), (float)FVector::DistSquared(MyLoc, M->GetActorLocation()) });

    SortedN.Sort([](const FMagnetDistSq& A, const FMagnetDistSq& B){ return A.DistSq < B.DistSq; });
    SortedS.Sort([](const FMagnetDistSq& A, const FMagnetDistSq& B){ return A.DistSq < B.DistSq; });

    // 스캔 사이에 자석이 파괴/null이 된 경우까지 대비해 루프 범위를 클램프
    const int32 PairLoop = FMath::Min(EffectivePairs, FMath::Min(SortedN.Num(), SortedS.Num()));

    float TotalB = 0.f;
    for (int32 i = 0; i < PairLoop; ++i)
    {
        AMagnet* MN = SortedN[i].Magnet;
        AMagnet* MS = SortedS[i].Magnet;

        const float DistN = FMath::Max(FMath::Sqrt(SortedN[i].DistSq), 1.f);
        const float DistS = FMath::Max(FMath::Sqrt(SortedS[i].DistSq), 1.f);

        const float BN = (MN->GetStrength() / 1000000.f)
                       * FMath::Pow(MN->GetReferenceDistance() / DistN, MN->GetDecayExponent());
        const float BS = (MS->GetStrength() / 1000000.f)
                       * FMath::Pow(MS->GetReferenceDistance() / DistS, MS->GetDecayExponent());
        TotalB += (BN + BS) * 0.5f;
    }

    // ── 5) EMF 계산 ─────────────────────────────────────────────
    const float AngleRad        = FMath::DegreesToRadians(RotationAngle);
    const float AngularVelocity = FMath::DegreesToRadians(CurrentRotationSpeed);
    const float CoilRadius      = CoilRadiusCM / 100.f;
    const float CoilArea        = PI * CoilRadius * CoilRadius;

    float EMF = CoilWindings * TotalB * CoilArea
              * AngularVelocity
              * FMath::Sin(AngleRad * (float)EffectivePairs);

    // ── 6) 불균형 페널티 ────────────────────────────────────────
    if (ImbalanceRatio > 0.f && ImbalancePenaltyScale > 0.f)
    {
        ImbalanceNoiseTime += DeltaTime;
        const float Noise = FMath::Sin(ImbalanceNoiseTime * 17.3f)
                          * FMath::Cos(ImbalanceNoiseTime * 11.7f);
        const float PenaltyFactor = 1.f
            - (ImbalanceRatio * ImbalancePenaltyScale * 0.5f * (1.f + Noise));
        EMF *= FMath::Max(PenaltyFactor, 0.f);
    }
    else
    {
        ImbalanceNoiseTime = 0.f;
    }

    CurrentEMF       = EMF;
    bCurrentPositive = (CurrentEMF >= 0.f);
}

void AGenerator::UpdateCircuit()
{
    const float AbsEMF      = FMath::Abs(CurrentEMF);
    const bool  bGenerating = (AbsEMF >= MinEMFThreshold);

    // ── 1) AssignedWires: 회전 부착 + 직접 전기 공급 ────────────
    for (TObjectPtr<AWire>& WirePtr : AssignedWires)
    {
        AWire* Wire = WirePtr.Get();
        if (!Wire) continue;

        USceneComponent* WireRoot = Wire->GetRootComponent();
        if (WireRoot && bRotateConnectedWires)
        {
            if (WireRoot->GetAttachParent() != GeneratorMesh)
                Wire->AttachToComponent(GeneratorMesh,
                    FAttachmentTransformRules::KeepWorldTransform);
        }
        else if (WireRoot && !bRotateConnectedWires
              && WireRoot->GetAttachParent() == GeneratorMesh)
        {
            Wire->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        }

        if (bGenerating && bCurrentPositive)
        {
            Wire->SetBatterySource(true);
            Wire->SetBatteryVoltage(AbsEMF);
            Wire->SetPowered(true);
        }
        else
        {
            Wire->SetBatterySource(false);
            Wire->SetBatteryVoltage(0.f);
            Wire->SetPowered(false);
        }
    }

    // ── 2) OutputBox: 이전 프레임 전선 전원 끊기 ────────────────
    for (TObjectPtr<AWire>& WirePtr : BoxPoweredWires)
    {
        AWire* Wire = WirePtr.Get();
        if (!Wire) continue;
        Wire->SetBatterySource(false);
        Wire->SetBatteryVoltage(0.f);
        Wire->SetPowered(false);
    }
    BoxPoweredWires.Reset();   // Empty() 대신 Reset() → 용량 유지

    if (!bCoilOutputEnabled || !bGenerating || !bCurrentPositive) return;

    // ── 3) OutputBox 안 전선 탐지 + 전기 중계 ───────────────────
    const FTransform BoxXform  = OutputBox->GetComponentTransform();
    const FVector    BoxCenter = BoxXform.GetLocation();
    const FQuat      BoxRot    = BoxXform.GetRotation();
    const FVector    BoxExtent = OutputBox->GetScaledBoxExtent();

    TArray<FOverlapResult> Hits;
    FCollisionQueryParams QParams(SCENE_QUERY_STAT(GeneratorOutputSense), false);
    QParams.AddIgnoredActor(this);

    GetWorld()->OverlapMultiByObjectType(
        Hits, BoxCenter, BoxRot,
        FCollisionObjectQueryParams::AllObjects,
        FCollisionShape::MakeBox(BoxExtent), QParams);

    for (const FOverlapResult& H : Hits)
    {
        AWire* Wire = Cast<AWire>(H.GetActor());
        if (!Wire) continue;

        const FVector StartPt = Wire->GetStartPointLocation();
        const FVector LocalPt = BoxXform.InverseTransformPosition(StartPt);
        if (FMath::Abs(LocalPt.X) > BoxExtent.X ||
            FMath::Abs(LocalPt.Y) > BoxExtent.Y ||
            FMath::Abs(LocalPt.Z) > BoxExtent.Z) continue;

        bool bAlreadyAssigned = false;
        for (const TObjectPtr<AWire>& AW : AssignedWires)
            if (AW.Get() == Wire) { bAlreadyAssigned = true; break; }
        if (bAlreadyAssigned) continue;

        Wire->SetBatterySource(true);
        Wire->SetBatteryVoltage(AbsEMF);
        Wire->SetPowered(true);

        BoxPoweredWires.Add(Wire);
    }
}

void AGenerator::UpdateGeneratorSound()
{
    // EMF는 sin으로 깜빡이므로, 안정적인 "발전 가능" 상태로 판단 (각도 무관)
    const bool bRunning = bGeneratorActive && (EffectivePairs > 0);

    // ── 상태가 바뀐 순간 처리 ──
    if (bRunning != bWasRunning)
    {
        bWasRunning = bRunning;

        GetWorld()->GetTimerManager().ClearTimer(GenSoundTimerHandle);

        // 켜지든 꺼지든, 재생 중이던 ON 루프는 일단 끊는다
        if (ActiveGenAudio)
        {
            ActiveGenAudio->Stop();
            ActiveGenAudio = nullptr;
        }

        // 꺼질 때: OFF 즉시 1회 재생하고 끝
        if (!bRunning)
        {
            if (TurningOffSound)
                UGameplayStatics::PlaySoundAtLocation(this, TurningOffSound, GetActorLocation());
            return;
        }
    }

    // ── 돌아가는 동안: ON 사운드가 안 울리면 (다시) 재생 → 연속 재생 보장 ──
    if (bRunning && TurningOnSound)
    {
        if (!ActiveGenAudio || !ActiveGenAudio->IsPlaying())
        {
            ActiveGenAudio = UGameplayStatics::SpawnSoundAttached(
                TurningOnSound, GetRootComponent());
        }
    }
}