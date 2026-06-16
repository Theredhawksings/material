#include "Generator.h"
#include "Magnet.h"
#include "Wire.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
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
}

void AGenerator::BeginPlay()
{
    Super::BeginPlay();
    DetectMagnets();
}

void AGenerator::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    DetectMagnets();
    UpdateEMF(DeltaTime);
    UpdateCircuit();

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
    NorthMagnets.Empty();
    SouthMagnets.Empty();

    TArray<AActor*> FoundMagnets;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMagnet::StaticClass(), FoundMagnets);

    for (AActor* Actor : FoundMagnets)
    {
        AMagnet* Magnet = Cast<AMagnet>(Actor);
        if (!Magnet || Magnet->IsDemagnetized()) continue;

        const float Dist = FVector::Dist(GetActorLocation(), Magnet->GetActorLocation());
        if (Dist > MagnetDetectRadius) continue;

        if (Magnet->IsNorthPole())
            NorthMagnets.Add(Magnet);
        else
            SouthMagnets.Add(Magnet);
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

    const FVector ToNorth = (NorthCentroid - GetActorLocation()).GetSafeNormal();
    const float Dot    = FVector::DotProduct(ToNorth, GetActorRightVector());
    const float RotDir = (Dot > 0.f) ? 1.f : -1.f;

    RotationAngle += RotDir * CurrentRotationSpeed * DeltaTime;
    if (RotationAngle >= 360.f)  RotationAngle -= 360.f;
    if (RotationAngle <= -360.f) RotationAngle += 360.f;

    GeneratorMesh->SetRelativeRotation((SpinAxisMask * RotationAngle).Quaternion());

    // ── 4) B(자기장): 거리 감쇠 + 쌍 합산 ──────────────────────
    TArray<AMagnet*> SortedN = NorthMagnets.FilterByPredicate(
        [](const TObjectPtr<AMagnet>& M){ return M != nullptr; });
    TArray<AMagnet*> SortedS = SouthMagnets.FilterByPredicate(
        [](const TObjectPtr<AMagnet>& M){ return M != nullptr; });

    SortedN.Sort([this](const AMagnet& A, const AMagnet& B){
        return FVector::Dist(GetActorLocation(), A.GetActorLocation())
             < FVector::Dist(GetActorLocation(), B.GetActorLocation());
    });
    SortedS.Sort([this](const AMagnet& A, const AMagnet& B){
        return FVector::Dist(GetActorLocation(), A.GetActorLocation())
             < FVector::Dist(GetActorLocation(), B.GetActorLocation());
    });

    float TotalB = 0.f;
    for (int32 i = 0; i < EffectivePairs; ++i)
    {
        const float StrengthN = SortedN[i]->GetStrength();
        const float StrengthS = SortedS[i]->GetStrength();
        const float DistN     = FMath::Max(
            FVector::Dist(GetActorLocation(), SortedN[i]->GetActorLocation()), 1.f);
        const float DistS     = FMath::Max(
            FVector::Dist(GetActorLocation(), SortedS[i]->GetActorLocation()), 1.f);

        // 각 자석의 DecayExponent와 ReferenceDistance 기반 감쇠
        const float DecayN   = SortedN[i]->GetDecayExponent();
        const float DecayS   = SortedS[i]->GetDecayExponent();
        const float RefDistN = SortedN[i]->GetReferenceDistance();
        const float RefDistS = SortedS[i]->GetReferenceDistance();

        const float BN = (StrengthN / 1000000.f) * FMath::Pow(RefDistN / DistN, DecayN);
        const float BS = (StrengthS / 1000000.f) * FMath::Pow(RefDistS / DistS, DecayS);
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
    BoxPoweredWires.Empty();

    if (!bCoilOutputEnabled || !bGenerating || !bCurrentPositive) return;

    // ── 3) OutputBox 안 전선 탐지 + 전기 중계 ───────────────────
    const FVector BoxCenter = OutputBox->GetComponentLocation();
    const FQuat   BoxRot    = OutputBox->GetComponentQuat();
    const FVector BoxExtent = OutputBox->GetScaledBoxExtent();

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
        const FVector LocalPt = OutputBox->GetComponentTransform()
                                         .InverseTransformPosition(StartPt);
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