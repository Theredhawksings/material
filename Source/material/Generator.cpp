#include "Generator.h"
#include "Magnet.h"
#include "Wire.h"
#include "Components/StaticMeshComponent.h"
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

    const FVector MyLoc     = GetActorLocation();
    const FVector DetectLoc = MyLoc + GetActorRotation().RotateVector(WireDetectOffset);

    DrawDebugSphere(GetWorld(), MyLoc,
        MagnetDetectRadius, 16, FColor::Yellow, false, -1.f, 0, 1.f);

    if (NorthMagnet && SouthMagnet)
    {
        DrawDebugSphere(GetWorld(), NorthMagnet->GetActorLocation(),
            40.f, 8, FColor::Red, false, -1.f, 0, 3.f);
        DrawDebugString(GetWorld(),
            NorthMagnet->GetActorLocation() + FVector(0, 0, 60.f),
            TEXT("N극"), nullptr, FColor::Red, 0.f, true);

        DrawDebugSphere(GetWorld(), SouthMagnet->GetActorLocation(),
            40.f, 8, FColor::Blue, false, -1.f, 0, 3.f);
        DrawDebugString(GetWorld(),
            SouthMagnet->GetActorLocation() + FVector(0, 0, 60.f),
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

        DrawDebugString(GetWorld(), MyLoc + FVector(0, 0, 100.f),
            FString::Printf(TEXT(
                "[Generator]\n"
                "%s\n"
                "회전각: %.1f\n"
                "EMF: %.2f V\n"
                "전류방향: %s\n"
                "연결Wire: %d개"
            ),
                *PolarityStr,
                RotationAngle,
                CurrentEMF,
                bCurrentPositive ? TEXT("정방향(+)") : TEXT("역방향(-)"),
                ConnectedWires.Num()
            ),
            nullptr, FColor::Green, 0.f, true);
    }
    else
    {
        DrawDebugString(GetWorld(), MyLoc + FVector(0, 0, 100.f),
            FString::Printf(TEXT(
                "[Generator]\n"
                "자석 탐색중...\n"
                "N극: %s\n"
                "S극: %s"
            ),
                NorthMagnet ? TEXT("찾음") : TEXT("없음"),
                SouthMagnet ? TEXT("찾음") : TEXT("없음")
            ),
            nullptr, FColor::Orange, 0.f, true);
    }

    DrawDebugSphere(GetWorld(), DetectLoc,
        WireDetectRadius, 12, FColor::Cyan, false, -1.f, 0, 1.f);
#endif
}

void AGenerator::DetectMagnets()
{
    NorthMagnet = nullptr;
    SouthMagnet = nullptr;

    TArray<AActor*> FoundMagnets;
    UGameplayStatics::GetAllActorsOfClass(
        GetWorld(), AMagnet::StaticClass(), FoundMagnets);

    for (AActor* Actor : FoundMagnets)
    {
        AMagnet* Magnet = Cast<AMagnet>(Actor);
        if (!Magnet || Magnet->IsDemagnetized()) continue;

        const float Dist = FVector::Dist(
            GetActorLocation(), Magnet->GetActorLocation());
        if (Dist > MagnetDetectRadius) continue;

        if (Magnet->IsNorthPole())
        {
            if (!NorthMagnet || Dist < FVector::Dist(
                GetActorLocation(), NorthMagnet->GetActorLocation()))
                NorthMagnet = Magnet;
        }
        else
        {
            if (!SouthMagnet || Dist < FVector::Dist(
                GetActorLocation(), SouthMagnet->GetActorLocation()))
                SouthMagnet = Magnet;
        }
    }
}

void AGenerator::UpdateEMF(float DeltaTime)
{
        // ★ 비활성화면 EMF 없음
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

    // 위에서 보면 빙글빙글 (Yaw)
    GeneratorMesh->SetRelativeRotation(FRotator(0.f, RotationAngle, 0.f));

    const float B = ((NorthMagnet->GetStrength()
                    + SouthMagnet->GetStrength()) * 0.5f)
                    / 1000000.f;

    const float AngleRad        = FMath::DegreesToRadians(RotationAngle);
    const float AngularVelocity = FMath::DegreesToRadians(RotationSpeed);
    const float CoilRadius      = CoilRadiusCM / 100.f;
    const float CoilArea        = PI * CoilRadius * CoilRadius;

    CurrentEMF = CoilWindings * B * CoilArea
               * AngularVelocity
               * FMath::Sin(AngleRad);

    // ★ 핵심: 매 프레임 정확하게 갱신
    bCurrentPositive = (CurrentEMF >= 0.f);
}

void AGenerator::UpdateCircuit()
{
    for (TObjectPtr<AWire>& WirePtr : ConnectedWires)
    {
        AWire* Wire = WirePtr.Get();
        if (!Wire) continue;
        Wire->SetBatterySource(false);
        Wire->SetPowered(false);
        Wire->SetBatteryVoltage(0.f);
    }
    ConnectedWires.Empty();

    const float AbsEMF = FMath::Abs(CurrentEMF);
    if (AbsEMF < MinEMFThreshold) return;

    // ★ 역방향이면 전원 끊고 종료
    if (!bCurrentPositive) return;

    const FVector DetectLoc = GetActorLocation()
        + GetActorRotation().RotateVector(WireDetectOffset);

    TArray<FOverlapResult> Hits;
    FCollisionQueryParams QParams(SCENE_QUERY_STAT(GeneratorWireSense), false);
    QParams.AddIgnoredActor(this);

    GetWorld()->OverlapMultiByObjectType(
        Hits, DetectLoc, FQuat::Identity,
        FCollisionObjectQueryParams::AllObjects,
        FCollisionShape::MakeSphere(WireDetectRadius), QParams);

    for (const FOverlapResult& H : Hits)
    {
        AWire* Wire = Cast<AWire>(H.GetActor());
        if (!Wire) continue;

        const float StartDist = FVector::Dist(
            DetectLoc, Wire->GetStartPointLocation());
        if (StartDist > WireDetectRadius) continue;

        Wire->SetBatterySource(true);
        Wire->SetBatteryVoltage(AbsEMF);
        Wire->SetPowered(true);

        Wire->Tags.Remove(FName("CurrentPositive"));
        Wire->Tags.Remove(FName("CurrentNegative"));
        Wire->Tags.Add(FName("CurrentPositive"));

        ConnectedWires.Add(Wire);
    }
}