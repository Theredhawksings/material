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

    // ★ OutputBox: Root에 붙어 있어서 에디터에서 위치를 자유롭게 옮길 수 있음
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
    const bool bOutputOn   = bCoilOutputEnabled && bGenerating;

    DrawDebugSphere(GetWorld(), MyLoc,
        MagnetDetectRadius, 16, FColor::Yellow, false, -1.f, 0, 1.f);

    DrawDebugBox(GetWorld(), BoxLoc,
        OutputBox->GetScaledBoxExtent(),
        OutputBox->GetComponentQuat(),
        bOutputOn ? FColor::Cyan : FColor(100, 100, 100),
        false, -1.f, 0, 2.f);

    if (NorthMagnet && SouthMagnet)
    {
        DrawDebugSphere(GetWorld(), NorthMagnet->GetActorLocation(),
            40.f, 8, FColor::Red, false, -1.f, 0, 3.f);
        DrawDebugString(GetWorld(),
            NorthMagnet->GetActorLocation() + FVector(0,0,60.f),
            TEXT("N극"), nullptr, FColor::Red, 0.f, true);

        DrawDebugSphere(GetWorld(), SouthMagnet->GetActorLocation(),
            40.f, 8, FColor::Blue, false, -1.f, 0, 3.f);
        DrawDebugString(GetWorld(),
            SouthMagnet->GetActorLocation() + FVector(0,0,60.f),
            TEXT("S극"), nullptr, FColor::Blue, 0.f, true);

        const FVector ToNorth =
            (NorthMagnet->GetActorLocation() - MyLoc).GetSafeNormal();
        const float Dot = FVector::DotProduct(ToNorth, GetActorRightVector());
        FString PolarityStr = (Dot > 0.f)
            ? TEXT("N극: 오른쪽 / S극: 왼쪽")
            : TEXT("N극: 왼쪽 / S극: 오른쪽");

        DrawDebugDirectionalArrow(GetWorld(),
            NorthMagnet->GetActorLocation(),
            SouthMagnet->GetActorLocation(),
            30.f, FColor::Magenta, false, -1.f, 0, 3.f);

        DrawDebugString(GetWorld(), MyLoc + FVector(0,0,100.f),
            FString::Printf(TEXT(
                "[Generator]\n%s\n회전각: %.1f\nEMF: %.2f V\n"
                "전류방향: %s\n체크박스: %s / 발전중: %s\n"
                "전기출력: %s\n박스내 Wire: %d개"),
                *PolarityStr, RotationAngle, CurrentEMF,
                bCurrentPositive ? TEXT("정방향(+)") : TEXT("역방향(-)"),
                bCoilOutputEnabled ? TEXT("ON") : TEXT("OFF"),
                bGenerating ? TEXT("예") : TEXT("아니오"),
                bOutputOn ? TEXT("ON") : TEXT("OFF"),
                BoxPoweredWires.Num()),
            nullptr, FColor::Green, 0.f, true);
    }
    else
    {
        DrawDebugString(GetWorld(), MyLoc + FVector(0,0,100.f),
            FString::Printf(TEXT("[Generator]\n자석 탐색중...\nN극: %s\nS극: %s"),
                NorthMagnet ? TEXT("찾음") : TEXT("없음"),
                SouthMagnet ? TEXT("찾음") : TEXT("없음")),
            nullptr, FColor::Orange, 0.f, true);
    }
#endif
}

void AGenerator::DetectMagnets()
{
    NorthMagnet = nullptr;
    SouthMagnet = nullptr;

    TArray<AActor*> FoundMagnets;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMagnet::StaticClass(), FoundMagnets);

    for (AActor* Actor : FoundMagnets)
    {
        AMagnet* Magnet = Cast<AMagnet>(Actor);
        if (!Magnet || Magnet->IsDemagnetized()) continue;

        const float Dist = FVector::Dist(GetActorLocation(), Magnet->GetActorLocation());
        if (Dist > MagnetDetectRadius) continue;

        if (Magnet->IsNorthPole())
        {
            if (!NorthMagnet || Dist < FVector::Dist(GetActorLocation(), NorthMagnet->GetActorLocation()))
                NorthMagnet = Magnet;
        }
        else
        {
            if (!SouthMagnet || Dist < FVector::Dist(GetActorLocation(), SouthMagnet->GetActorLocation()))
                SouthMagnet = Magnet;
        }
    }
}

void AGenerator::UpdateEMF(float DeltaTime)
{
    if (!bGeneratorActive)
    {
        CurrentEMF    = 0.f;
        RotationAngle = 0.f;
        GeneratorMesh->SetRelativeRotation(FRotator::ZeroRotator);
        return;
    }

    if (!NorthMagnet || !SouthMagnet)
    {
        CurrentEMF    = 0.f;
        RotationAngle = 0.f;
        return;
    }

    if (NorthMagnet->IsDemagnetized() || SouthMagnet->IsDemagnetized())
    {
        CurrentEMF = 0.f;
        return;
    }

    const FVector ToNorth =
        (NorthMagnet->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    const float Dot    = FVector::DotProduct(ToNorth, GetActorRightVector());
    const float RotDir = (Dot > 0.f) ? 1.f : -1.f;

    RotationAngle += RotDir * RotationSpeed * DeltaTime;
    if (RotationAngle >= 360.f)  RotationAngle -= 360.f;
    if (RotationAngle <= -360.f) RotationAngle += 360.f;

    GeneratorMesh->SetRelativeRotation((SpinAxisMask * RotationAngle).Quaternion());

    const float B = ((NorthMagnet->GetStrength() + SouthMagnet->GetStrength()) * 0.5f) / 1000000.f;
    const float AngleRad        = FMath::DegreesToRadians(RotationAngle);
    const float AngularVelocity = FMath::DegreesToRadians(RotationSpeed);
    const float CoilRadius      = CoilRadiusCM / 100.f;
    const float CoilArea        = PI * CoilRadius * CoilRadius;

    CurrentEMF       = CoilWindings * B * CoilArea * AngularVelocity * FMath::Sin(AngleRad);
    bCurrentPositive = (CurrentEMF >= 0.f);
}

void AGenerator::UpdateCircuit()
{
    const float AbsEMF    = FMath::Abs(CurrentEMF);
    const bool  bGenerating = (AbsEMF >= MinEMFThreshold);

    // ── 1) AssignedWires: 발전기 본체에 붙어 회전 + 직접 전기 공급 ──
    for (TObjectPtr<AWire>& WirePtr : AssignedWires)
    {
        AWire* Wire = WirePtr.Get();
        if (!Wire) continue;

        // 발전기 본체에 부착해서 같이 회전
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

        // 발전기가 전기를 만들면 이 전선에 직접 공급
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

    // ── 2) OutputBox: 이전 프레임 전선 전원 끊기 ──────────────────
    for (TObjectPtr<AWire>& WirePtr : BoxPoweredWires)
    {
        AWire* Wire = WirePtr.Get();
        if (!Wire) continue;
        Wire->SetBatterySource(false);
        Wire->SetBatteryVoltage(0.f);
        Wire->SetPowered(false);
    }
    BoxPoweredWires.Empty();

    // 체크박스 OFF 또는 발전 안 하면 OutputBox 중계 없음
    if (!bCoilOutputEnabled || !bGenerating || !bCurrentPositive) return;

    // ── 3) OutputBox 안에 있는 전선의 시작점(ConnectionSphere) 찾기 ──
    const FVector BoxCenter = OutputBox->GetComponentLocation();
    const FQuat   BoxRot    = OutputBox->GetComponentQuat();
    const FVector BoxExtent = OutputBox->GetScaledBoxExtent();

    TArray<FOverlapResult> Hits;
    FCollisionQueryParams QParams(SCENE_QUERY_STAT(GeneratorOutputSense), false);
    QParams.AddIgnoredActor(this);

    GetWorld()->OverlapMultiByObjectType(
        Hits, BoxCenter, BoxRot,
        FCollisionObjectQueryParams::AllObjects,
        FCollisionShape::MakeBox(BoxExtent),
        QParams);

    for (const FOverlapResult& H : Hits)
    {
        AWire* Wire = Cast<AWire>(H.GetActor());
        if (!Wire) continue;

        // ★ 전선의 시작점(ConnectionSphere)이 박스 안에 있는지 확인
        const FVector StartPt = Wire->GetStartPointLocation();
        const FVector LocalPt = OutputBox->GetComponentTransform().InverseTransformPosition(StartPt);
        if (FMath::Abs(LocalPt.X) > BoxExtent.X ||
            FMath::Abs(LocalPt.Y) > BoxExtent.Y ||
            FMath::Abs(LocalPt.Z) > BoxExtent.Z) continue;

        // AssignedWires에 이미 포함된 전선은 스킵 (중복 공급 방지)
        bool bAlreadyAssigned = false;
        for (const TObjectPtr<AWire>& AW : AssignedWires)
            if (AW.Get() == Wire) { bAlreadyAssigned = true; break; }
        if (bAlreadyAssigned) continue;

        // ★ 발전기 전기값을 이 전선으로 중계
        Wire->SetBatterySource(true);
        Wire->SetBatteryVoltage(AbsEMF);
        Wire->SetPowered(true);

        BoxPoweredWires.Add(Wire);
    }
}