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

    // 루트는 고정 씬 컴포넌트
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // 메시만 루트에 붙여서 독립적으로 회전
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

    const FVector MyLoc = GetActorLocation();

    // 자석 감지 범위
    DrawDebugSphere(GetWorld(), MyLoc,
        MagnetDetectRadius, 16, FColor::Yellow, false, -1.f, 0, 1.f);

    if (NorthMagnet && SouthMagnet)
    {
        // N극 표시
        DrawDebugSphere(GetWorld(), NorthMagnet->GetActorLocation(),
            40.f, 8, FColor::Red, false, -1.f, 0, 3.f);
        DrawDebugString(GetWorld(),
            NorthMagnet->GetActorLocation() + FVector(0, 0, 60.f),
            TEXT("N극"), nullptr, FColor::Red, 0.f, true);

        // S극 표시
        DrawDebugSphere(GetWorld(), SouthMagnet->GetActorLocation(),
            40.f, 8, FColor::Blue, false, -1.f, 0, 3.f);
        DrawDebugString(GetWorld(),
            SouthMagnet->GetActorLocation() + FVector(0, 0, 60.f),
            TEXT("S극"), nullptr, FColor::Blue, 0.f, true);

        // N극이 왼쪽인지 오른쪽인지 판단
        const FVector ToNorth =
            (NorthMagnet->GetActorLocation() - MyLoc).GetSafeNormal();
        const float Dot = FVector::DotProduct(ToNorth, GetActorRightVector());

        FString PolarityStr = (Dot > 0.f)
            ? TEXT("N극: 오른쪽 / S극: 왼쪽")
            : TEXT("N극: 왼쪽 / S극: 오른쪽");

        // 자기장 방향 화살표 (N→S)
        DrawDebugDirectionalArrow(GetWorld(),
            NorthMagnet->GetActorLocation(),
            SouthMagnet->GetActorLocation(),
            30.f, FColor::Magenta, false, -1.f, 0, 3.f);

        // EMF 정보
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
        // 자석 못 찾음
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

    // Wire 탐지 범위
    DrawDebugSphere(GetWorld(), MyLoc,
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

    // 회전각 업데이트
    RotationAngle += RotationSpeed * DeltaTime;
    if (RotationAngle >= 360.f) RotationAngle -= 360.f;

    // GeneratorMesh만 회전 (루트는 고정)
    GeneratorMesh->SetRelativeRotation(
        FRotator(RotationAngle, 0.f, 0.f));

    // 자기장 세기 (두 자석 평균, 게임 스케일 조정)
    const float B = ((NorthMagnet->GetStrength()
                    + SouthMagnet->GetStrength()) * 0.5f)
                    / 1000000.f;

    // EMF = N * B * A * ω * sin(θ)
    const float AngleRad        = FMath::DegreesToRadians(RotationAngle);
    const float AngularVelocity = FMath::DegreesToRadians(RotationSpeed);
    const float CoilRadius      = CoilRadiusCM / 100.f;
    const float CoilArea        = PI * CoilRadius * CoilRadius;

    CurrentEMF = CoilWindings * B * CoilArea
               * AngularVelocity
               * FMath::Sin(AngleRad);

    bCurrentPositive = (CurrentEMF >= 0.f);
}

void AGenerator::UpdateCircuit()
{
    // 이전 Wire 전원 끊기
    for (TObjectPtr<AWire>& WirePtr : ConnectedWires)
    {
        AWire* Wire = WirePtr.Get();
        if (!Wire) continue;
        Wire->SetBatterySource(false);
        Wire->SetPowered(false);
        Wire->SetBatteryVoltage(0.f);
    }
    ConnectedWires.Empty();

    // EMF 없으면 종료
    const float AbsEMF = FMath::Abs(CurrentEMF);
    if (AbsEMF < MinEMFThreshold) return;

    // 주변 Wire 탐지
    TArray<FOverlapResult> Hits;
    FCollisionQueryParams QParams(SCENE_QUERY_STAT(GeneratorWireSense), false);
    QParams.AddIgnoredActor(this);

    GetWorld()->OverlapMultiByObjectType(
        Hits, GetActorLocation(), FQuat::Identity,
        FCollisionObjectQueryParams::AllObjects,
        FCollisionShape::MakeSphere(WireDetectRadius), QParams);

    for (const FOverlapResult& H : Hits)
    {
        AWire* Wire = Cast<AWire>(H.GetActor());
        if (!Wire) continue;

        // 업스트림(시작점)이 Generator 근처인지 확인
        const float StartDist = FVector::Dist(
            GetActorLocation(), Wire->GetStartPointLocation());
        if (StartDist > WireDetectRadius) continue;

        // 전력 주입
        Wire->SetBatterySource(true);
        Wire->SetBatteryVoltage(AbsEMF);
        Wire->SetPowered(true);

        // 전류 방향 태그
        Wire->Tags.Remove(FName("CurrentPositive"));
        Wire->Tags.Remove(FName("CurrentNegative"));
        Wire->Tags.Add(bCurrentPositive
            ? FName("CurrentPositive")
            : FName("CurrentNegative"));

        ConnectedWires.Add(Wire);
    }
}