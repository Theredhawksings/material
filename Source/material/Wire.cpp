#include "Wire.h"
#include "Transformation_actor.h"
#include "Temperature.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"

// ─────────────────────────────────────────────────────────────────────────────
// 생성자
// ─────────────────────────────────────────────────────────────────────────────

AWire::AWire()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
    Spline->SetupAttachment(Root);

    // 시작점 연결 구체
    ConnectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ConnectionSphere"));
    ConnectionSphere->SetupAttachment(Root);
    ConnectionSphere->SetSphereRadius(OverlapRadius);
    ConnectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ConnectionSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
    ConnectionSphere->SetCollisionObjectType(ECC_GameTraceChannel2);

    // 끝점 연결 구체
    ConnectionSphereEnd = CreateDefaultSubobject<USphereComponent>(TEXT("ConnectionSphereEnd"));
    ConnectionSphereEnd->SetupAttachment(Root);
    ConnectionSphereEnd->SetSphereRadius(OverlapRadius);
    ConnectionSphereEnd->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ConnectionSphereEnd->SetCollisionResponseToAllChannels(ECR_Overlap);
    ConnectionSphereEnd->SetCollisionObjectType(ECC_GameTraceChannel2);
}

// ─────────────────────────────────────────────────────────────────────────────
// 라이프사이클
// ─────────────────────────────────────────────────────────────────────────────

void AWire::BeginPlay()
{
    Super::BeginPlay();

    UpdateConnectionPoint();
    ApplyPower();
    ApplyDebugVisibility();

    GetWorldTimerManager().SetTimerForNextTick(this, &AWire::RefreshConnectedActors);

    if (RefreshInterval > 0.f)
    {
        GetWorldTimerManager().SetTimer(RefreshTimerHandle, this,
            &AWire::RefreshConnectedActors, RefreshInterval, true);
    }
}

void AWire::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(RefreshTimerHandle);
    Super::EndPlay(EndPlayReason);
}

void AWire::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdateConnectionPoint();
    RebuildSplineMeshes();
}

void AWire::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateJouleHeating(DeltaTime);

    if (WireTemperatureC > HeatEmitThresholdC)
    {
        HeatEmitAccumulator += DeltaTime;
        if (HeatEmitAccumulator >= HeatEmitInterval)
        {
            EmitHeatToNearby(HeatEmitAccumulator);
            HeatEmitAccumulator = 0.f;
        }
    }
    else
    {
        HeatEmitAccumulator = 0.f;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 회로 해석 (2패스)
//
//  패스 1 - BuildCircuitGraph
//    전체 네트워크를 DFS 탐색해서
//    각 전선에 "몇 개의 상위 경로가 들어오는지(IncomingCount)" 기록
//    → IncomingCount > 1 이면 병렬 합산 지점(다이아몬드 합류점)
//
//  패스 2 - PropagateVoltage
//    소스에서 출발해 전압/전류를 흘림
//    합류 지점은 모든 경로가 도달해야 비로소 다음 전선으로 전파
//    → 전류는 합산, 전압은 동일 유지
// ─────────────────────────────────────────────────────────────────────────────

void AWire::BuildCircuitGraph(TMap<AWire*, int32>& IncomingCountMap,
                               TSet<AWire*>&        Visited)
{
    if (Visited.Contains(this)) return;
    Visited.Add(this);

    for (AWire* Next : ConnectedWires)
    {
        if (!Next) continue;

        // 이 전선을 가리키는 경로 수 +1
        int32& Count = IncomingCountMap.FindOrAdd(Next, 0);
        Count++;

        if (!Visited.Contains(Next))
            Next->BuildCircuitGraph(IncomingCountMap, Visited);
    }
}

void AWire::PropagateVoltage(float IncomingVoltage,
                              float IncomingCurrent,
                              TMap<AWire*, float>&       VoltageMap,
                              TMap<AWire*, float>&       CurrentAccumMap,
                              const TMap<AWire*, int32>& IncomingCountMap)
{
    // ── 병렬 합산 지점 처리 ──────────────────────────────────
    // 이 전선에 들어오는 경로가 여러 개라면
    // 모든 경로가 도달할 때까지 전류를 누적하고 기다림
    const int32* ExpectedCount = IncomingCountMap.Find(this);
    if (ExpectedCount && *ExpectedCount > 1)
    {
        float& AccumCurrent = CurrentAccumMap.FindOrAdd(this, 0.f);
        AccumCurrent += IncomingCurrent;

        if (VoltageMap.Contains(this))
        {
            // 이미 전압이 설정돼 있음 → 전류만 누적하고 종료
            // 전파는 첫 방문자가 담당
            EffectiveCurrent = AccumCurrent;
            return;
        }

        // 첫 방문: 전압 설정 후 계속 전파
        VoltageMap.Add(this, IncomingVoltage);
        EffectiveVoltage = IncomingVoltage;
        EffectiveCurrent = AccumCurrent; // 지금까지 누적된 전류
    }
    else
    {
        // 단일 경로 → 이미 방문했으면 스킵 (무한 루프 방지)
        if (VoltageMap.Contains(this)) return;

        VoltageMap.Add(this, IncomingVoltage);
        EffectiveVoltage = IncomingVoltage;
        EffectiveCurrent = IncomingCurrent;
    }

    // ── 이 전선의 발열에 쓸 전류 갱신 ──────────────────────
    // Joule: P = I²R, 전류는 지금까지 계산된 EffectiveCurrent 사용

    // ── 다음 전선으로 전파 ──────────────────────────────────
    TArray<AWire*> NextWires;
    for (AWire* W : ConnectedWires)
        if (W && !VoltageMap.Contains(W))
            NextWires.Add(W);

    if (NextWires.Num() == 0)
    {
        // 회로 끝 - 전류는 EffectiveVoltage / R
        EffectiveCurrent = EffectiveVoltage / FMath::Max(Resistance, 0.01f);
        return;
    }

    if (NextWires.Num() == 1)
    {
        // ── 직렬 ──────────────────────────────────────────
        // 전류 = 전압 / 내 저항
        // 다음 전선 전압 = 현재 전압 - 내 저항에서의 강하
        const float I  = EffectiveVoltage / FMath::Max(Resistance, 0.01f);
        EffectiveCurrent = I;
        const float Drop = I * Resistance;
        const float NextV = FMath::Max(EffectiveVoltage - Drop, 0.f);

#if ENABLE_DRAW_DEBUG
        if (bDebugCircuit)
        {
            DrawDebugString(GetWorld(),
                GetActorLocation() + FVector(0.f, 0.f, 130.f),
                FString::Printf(TEXT("[직렬] V: %.1f→%.1f  I: %.2fA  R: %.1fΩ"),
                    EffectiveVoltage, NextV, I, Resistance),
                nullptr, FColor::Cyan, 0.f, true);
        }
#endif

        NextWires[0]->SetPowered(true);
        NextWires[0]->PropagateVoltage(NextV, I, VoltageMap, CurrentAccumMap, IncomingCountMap);
    }
    else
    {
        // ── 병렬 분기 ─────────────────────────────────────
        // 각 가지에 같은 전압, 전류는 저항 반비례로 분배

        // 병렬 합성 저항
        float InvRSum = 0.f;
        for (AWire* Next : NextWires)
            InvRSum += 1.f / FMath::Max(Next->Resistance, 0.01f);
        const float ParallelR   = (InvRSum > 0.f) ? (1.f / InvRSum) : 0.01f;
        const float TotalCurrent = EffectiveVoltage / FMath::Max(ParallelR, 0.01f);
        EffectiveCurrent = TotalCurrent;

#if ENABLE_DRAW_DEBUG
        if (bDebugCircuit)
        {
            DrawDebugString(GetWorld(),
                GetActorLocation() + FVector(0.f, 0.f, 130.f),
                FString::Printf(TEXT("[병렬x%d] V: %.1f  I_합: %.2fA  R합성: %.2fΩ"),
                    NextWires.Num(), EffectiveVoltage, TotalCurrent, ParallelR),
                nullptr, FColor::Green, 0.f, true);
        }
#endif

        for (AWire* Next : NextWires)
        {
            // 각 가지 전류 = V / R_가지
            const float BranchI = EffectiveVoltage / FMath::Max(Next->Resistance, 0.01f);
            Next->SetPowered(true);
            Next->PropagateVoltage(EffectiveVoltage, BranchI,
                VoltageMap, CurrentAccumMap, IncomingCountMap);
        }
    }
}

void AWire::ResetVoltageNetwork(TSet<AWire*>& Visited)
{
    if (Visited.Contains(this)) return;
    Visited.Add(this);

    EffectiveVoltage = 0.f;
    EffectiveCurrent = 0.f;

    for (AWire* W : ConnectedWires)
        if (W && !Visited.Contains(W))
            W->ResetVoltageNetwork(Visited);
}

// ─────────────────────────────────────────────────────────────────────────────
// 소스 탐색 후 2패스 회로 해석 실행
// ─────────────────────────────────────────────────────────────────────────────

void AWire::TriggerCircuitSolve()
{
    // BFS로 소스 전선 탐색
    AWire* SourceWire = nullptr;

    TSet<AWire*> Searched;
    TQueue<AWire*> Queue;
    Queue.Enqueue(this);
    Searched.Add(this);

    while (!Queue.IsEmpty())
    {
        AWire* Cur = nullptr;
        Queue.Dequeue(Cur);
        if (!Cur) continue;

        if (Cur->bPoweredBySource)
        {
            SourceWire = Cur;
            break;
        }

        for (AWire* Neighbor : Cur->ConnectedWires)
            if (Neighbor && !Searched.Contains(Neighbor))
            {
                Searched.Add(Neighbor);
                Queue.Enqueue(Neighbor);
            }
    }

    if (!SourceWire) return;

    // 패스 1: 그래프 빌드 (들어오는 경로 수 카운트)
    TMap<AWire*, int32> IncomingCountMap;
    TSet<AWire*> GraphVisited;
    SourceWire->BuildCircuitGraph(IncomingCountMap, GraphVisited);

    // 패스 2: 전압/전류 전파
    TMap<AWire*, float> VoltageMap;
    TMap<AWire*, float> CurrentAccumMap;

    const float SourceV = (SourceWire->BatteryVoltage > 0.f)
        ? SourceWire->BatteryVoltage
        : SourceWire->DefaultVoltage;

    SourceWire->PropagateVoltage(SourceV, SourceV / FMath::Max(SourceWire->Resistance, 0.01f),
        VoltageMap, CurrentAccumMap, IncomingCountMap);
}

// ─────────────────────────────────────────────────────────────────────────────
// 전원 상태 관리
// ─────────────────────────────────────────────────────────────────────────────

void AWire::UpdateFinalPower()
{
    const bool bNewFinal = (bPoweredBySource || bPoweredByMetal);
    if (bPoweredFinal == bNewFinal) return;

    bPoweredFinal = bNewFinal;
    ApplyPower();
    PropagatePowerToConnected();

    if (!bPoweredFinal)
    {
        BatteryVoltage   = 0.f;
        EffectiveVoltage = 0.f;
        EffectiveCurrent = 0.f;

        TSet<AWire*> Visited;
        Visited.Add(this);
        for (AWire* W : ConnectedWires)
            if (W && !Visited.Contains(W))
                W->ResetVoltageNetwork(Visited);

        RefreshConnectedActors();
    }
    else
    {
        TriggerCircuitSolve();
    }
}

void AWire::SetPowered(bool bNewPowered)
{
    if (bPoweredBySource == bNewPowered) return;
    bPoweredBySource = bNewPowered;
    if (!bNewPowered) bPoweredByMetal = false;
    UpdateFinalPower();
}

void AWire::SetPoweredByMetal(bool bNewPoweredByMetal)
{
    if (bPoweredByMetal == bNewPoweredByMetal) return;
    bPoweredByMetal = bNewPoweredByMetal;
    if (!bPoweredByMetal) BatteryVoltage = 0.f;
    UpdateFinalPower();
}

void AWire::SetBatteryVoltage(float NewVoltage)
{
    BatteryVoltage = FMath::Max(NewVoltage, 0.f);
}

// ─────────────────────────────────────────────────────────────────────────────
// 줄 가열 - EffectiveVoltage 기반
// ─────────────────────────────────────────────────────────────────────────────

void AWire::UpdateJouleHeating(float DeltaTime)
{
    if (bPoweredFinal)
    {
        const float R = FMath::Max(Resistance, 0.01f);

        // 회로 계산된 전압 우선, 없으면 폴백
        float V = 0.f;
        if      (EffectiveVoltage > 0.f) V = EffectiveVoltage;
        else if (BatteryVoltage   > 0.f) V = BatteryVoltage;
        else                             V = DefaultVoltage;

        CurrentAmps = V / R;

        const float JoulePowerW = CurrentAmps * CurrentAmps * R;
        const float EnergyJ     = JoulePowerW * DeltaTime * FMath::Max(SimTimeScale, 0.f);
        const float DeltaT      = EnergyJ / FMath::Max(WireMassKg * SpecificHeatJPerKgK, 0.01f);
        WireTemperatureC += DeltaT;
    }
    else
    {
        CurrentAmps = 0.f;
    }

    if (WireTemperatureC > AmbientTemperatureC)
    {
        const float TempDiff  = WireTemperatureC - AmbientTemperatureC;
        const float CoolAmount = CoolingRateKPerSec * (TempDiff / 100.f) * DeltaTime;
        WireTemperatureC -= CoolAmount;
        WireTemperatureC  = FMath::Max(WireTemperatureC, AmbientTemperatureC);
    }

    WireTemperatureC = FMath::Clamp(WireTemperatureC, AmbientTemperatureC, MaxWireTemperatureC);

    UpdateWireVisual();

#if ENABLE_DRAW_DEBUG
    if (bDebugWire && WireTemperatureC > AmbientTemperatureC + 1.f)
    {
        const FColor TempColor = (WireTemperatureC > 400.f) ? FColor::Red
                                : (WireTemperatureC > 200.f) ? FColor::Orange
                                : FColor::Yellow;

        DrawDebugString(GetWorld(),
            GetActorLocation() + FVector(0.f, 0.f, 80.f),
            FString::Printf(TEXT("%.0f°C  %.1fA  V:%.1f"),
                WireTemperatureC, CurrentAmps, EffectiveVoltage),
            nullptr, TempColor, 0.0f, true);
    }
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// 연결 액터 갱신 + 전선끼리 연결 감지
// ─────────────────────────────────────────────────────────────────────────────

void AWire::RefreshConnectedActors()
{
    ConnectedActors.Empty();
    ConnectedWires.Empty();
    bool bFoundPower = false;

    // ── 세그먼트 오버랩 ──────────────────────────────────────
    for (USplineMeshComponent* Segment : SegmentMeshes)
    {
        if (!Segment) continue;

        TArray<AActor*> OverlappingActors;
        Segment->GetOverlappingActors(OverlappingActors);

        for (AActor* A : OverlappingActors)
        {
            if (!A || A == this) continue;

            if (AWire* OtherWire = Cast<AWire>(A))
            {
                ConnectedWires.AddUnique(OtherWire);
                if (OtherWire->IsPowered()) bFoundPower = true;
                continue;
            }

            if (A->ActorHasTag(FName("Metal")) || A->ActorHasTag(FName("Copper")))
            {
                const FVector SegMidWorld = Segment->GetComponentLocation();
                const float   Dist        = FVector::Dist(SegMidWorld, A->GetActorLocation());
                if (Dist > OverlapRadius * 3.f) continue;

                if (ATransformation_actor* Conductor = Cast<ATransformation_actor>(A))
                {
                    ConnectedActors.AddUnique(Conductor);
                    if (Conductor->IsElectrified()) bFoundPower = true;
                }
            }
        }
    }

    // ── 끝점 구체 오버랩 (시작점 / 끝점에서 전선 연결 감지) ──
    auto CheckEndpoint = [&](USphereComponent* Sphere)
    {
        if (!Sphere) return;
        TArray<AActor*> Overlapping;
        Sphere->GetOverlappingActors(Overlapping);

        for (AActor* A : Overlapping)
        {
            if (!A || A == this) continue;
            if (AWire* OtherWire = Cast<AWire>(A))
            {
                ConnectedWires.AddUnique(OtherWire);
                if (OtherWire->IsPowered()) bFoundPower = true;
            }
        }
    };

    CheckEndpoint(ConnectionSphere);     // 시작점
    CheckEndpoint(ConnectionSphereEnd);  // 끝점

    SetPoweredByMetal(bFoundPower);

    // 소스 전선이면 전체 회로 재계산
    if (bPoweredBySource && bPoweredFinal)
        TriggerCircuitSolve();
}

// ─────────────────────────────────────────────────────────────────────────────
// 열 방출
// ─────────────────────────────────────────────────────────────────────────────

void AWire::EmitHeatToNearby(float DeltaTime)
{
    if (!GetWorld()) return;

    const float T_K       = WireTemperatureC + 273.15f;
    const float EmitPowerW = WireEmissivity * StefanBoltzmannSigma * WireSurfaceAreaM2
                            * FMath::Pow(T_K, 4.f);
    if (EmitPowerW <= 0.f) return;

    FCollisionObjectQueryParams ObjParams;
    ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjParams.AddObjectTypesToQuery(ECC_PhysicsBody);

    FCollisionQueryParams QParams(SCENE_QUERY_STAT(WireHeatOverlap), false);
    QParams.AddIgnoredActor(this);

    CachedHeatTargets.Reset();

    auto HeatAt = [&](const FVector& Center, float Radius, float Multiplier)
    {
        TArray<FOverlapResult> Hits;
        GetWorld()->OverlapMultiByObjectType(
            Hits, Center, FQuat::Identity, ObjParams,
            FCollisionShape::MakeSphere(Radius), QParams);

        for (const FOverlapResult& H : Hits)
        {
            ATransformation_actor* Ice = Cast<ATransformation_actor>(H.GetActor());
            if (!Ice || CachedHeatTargets.Contains(Ice)) continue;

            const float DistCm  = FVector::Dist(Center, Ice->GetActorLocation());
            const float DistM   = FMath::Max(DistCm / 100.f, 0.05f);
            const float FluxWm2 = EmitPowerW / (4.f * PI * DistM * DistM);
            const float Fade    = FMath::Clamp(1.f - (DistCm / Radius), 0.f, 1.f);
            const float EnergyJ = FluxWm2 * IceReceiveAreaM2 * Fade * DeltaTime * Multiplier;

            if (EnergyJ > 0.f)
            {
                Ice->ReceiveHeatEnergy(EnergyJ, WireTemperatureC);
                CachedHeatTargets.Add(Ice);
            }
        }
    };

    for (USphereComponent* Sphere : HeatSpheres)
        if (Sphere) HeatAt(Sphere->GetComponentLocation(), HeatEmitRadius, SegmentHeatMultiplier);

    if (IceHeatZone && WireTemperatureC >= IceHeatThresholdC)
        HeatAt(IceHeatZone->GetComponentLocation(), IceHeatZoneRadius, IceHeatMultiplier);
}

void AWire::OnIceHeatZoneBeginOverlap(UPrimitiveComponent*, AActor*, UPrimitiveComponent*,
    int32, bool, const FHitResult&) {}

void AWire::OnIceHeatZoneEndOverlap(UPrimitiveComponent*, AActor*, UPrimitiveComponent*,
    int32) {}

// ─────────────────────────────────────────────────────────────────────────────
// 연결 포인트 업데이트
// ─────────────────────────────────────────────────────────────────────────────

void AWire::UpdateConnectionPoint()
{
    if (!Spline) return;

    const int32 NumPoints = Spline->GetNumberOfSplinePoints();
    if (NumPoints < 1) return;

    if (ConnectionSphere)
    {
        const FVector StartLocal = Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local);
        ConnectionSphere->SetRelativeLocation(StartLocal);
        ConnectionSphere->SetSphereRadius(OverlapRadius);
    }

    if (ConnectionSphereEnd)
    {
        const FVector EndLocal = Spline->GetLocationAtSplinePoint(NumPoints - 1, ESplineCoordinateSpace::Local);
        ConnectionSphereEnd->SetRelativeLocation(EndLocal);
        ConnectionSphereEnd->SetSphereRadius(OverlapRadius);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 비주얼
// ─────────────────────────────────────────────────────────────────────────────

void AWire::ApplyPower()
{
    if (bPoweredFinal && OnMaterial)
    {
        if (SegmentMIDs.Num() == SegmentMeshes.Num() && bLastAppliedPowerState) return;

        SegmentMIDs.Empty();
        for (USplineMeshComponent* Mesh : SegmentMeshes)
        {
            if (!IsValid(Mesh)) continue;
            UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(OnMaterial, this);
            Mesh->SetMaterial(0, MID);
            SegmentMIDs.Add(MID);
        }
    }
    else
    {
        SegmentMIDs.Empty();
        for (USplineMeshComponent* Mesh : SegmentMeshes)
        {
            if (!IsValid(Mesh) || !OffMaterial) continue;
            for (int32 i = 0; i < Mesh->GetNumMaterials(); ++i)
                Mesh->SetMaterial(i, OffMaterial);
        }
    }
    bLastAppliedPowerState = bPoweredFinal;
}

void AWire::UpdateWireVisual()
{
    const float Alpha     = FMath::Clamp(WireTemperatureC * WireTempVisualScale, 0.f, 1.f);
    const float TempRatio = FMath::Clamp(
        (WireTemperatureC - AmbientTemperatureC) / (MaxWireTemperatureC - AmbientTemperatureC),
        0.f, 1.f);
    const int32 StencilVal = FMath::RoundToInt(TempRatio * 255.f);

    if (StencilVal == CachedWireStencilValue &&
        FMath::Abs(Alpha - CachedWireHeatAlpha) < 0.001f)
        return;

    CachedWireHeatAlpha    = Alpha;
    CachedWireStencilValue = StencilVal;

    for (UMaterialInstanceDynamic* MID : SegmentMIDs)
        if (MID) MID->SetScalarParameterValue(WireHeatParamName, Alpha);

    for (USplineMeshComponent* Mesh : SegmentMeshes)
        if (Mesh) Mesh->SetCustomDepthStencilValue(StencilVal);
}

void AWire::ApplyDebugVisibility()
{
    if (IceHeatZone)
    {
        IceHeatZone->SetHiddenInGame(!bShowDebugShapes);
        IceHeatZone->SetVisibility(bShowDebugShapes);
        IceHeatZone->bDrawOnlyIfSelected = !bShowDebugShapes;
        IceHeatZone->MarkRenderStateDirty();
    }

    for (USphereComponent* Sphere : HeatSpheres)
    {
        if (!Sphere) continue;
        Sphere->SetHiddenInGame(!bShowDebugShapes);
        Sphere->SetVisibility(bShowDebugShapes);
        Sphere->bDrawOnlyIfSelected = !bShowDebugShapes;
        Sphere->MarkRenderStateDirty();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 에디터
// ─────────────────────────────────────────────────────────────────────────────

#if WITH_EDITOR
void AWire::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropName = (PropertyChangedEvent.Property != nullptr)
        ? PropertyChangedEvent.Property->GetFName() : NAME_None;

    if (PropName == GET_MEMBER_NAME_CHECKED(AWire, bShowDebugShapes))
        ApplyDebugVisibility();

    if (PropName == GET_MEMBER_NAME_CHECKED(AWire, IceHeatZoneRadius))
    {
        if (IceHeatZone)
        {
            IceHeatZone->SetSphereRadius(IceHeatZoneRadius);
            IceHeatZone->MarkRenderStateDirty();
        }
    }
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
// 메시 빌드
// ─────────────────────────────────────────────────────────────────────────────

void AWire::ClearGeneratedMeshes()
{
    for (USplineMeshComponent* Comp : SegmentMeshes)
        if (Comp) { Comp->UnregisterComponent(); Comp->DestroyComponent(); }
    SegmentMeshes.Empty();
    SegmentMIDs.Empty();

    for (USphereComponent* Sphere : HeatSpheres)
        if (Sphere) { Sphere->UnregisterComponent(); Sphere->DestroyComponent(); }
    HeatSpheres.Empty();

    if (IceHeatZone)
    {
        IceHeatZone->UnregisterComponent();
        IceHeatZone->DestroyComponent();
        IceHeatZone = nullptr;
    }
}

void AWire::RebuildSplineMeshes()
{
    ClearGeneratedMeshes();
    if (!Spline || !SegmentMesh) return;

    const int32 NumPoints = Spline->GetNumberOfSplinePoints();
    if (NumPoints < 2) return;

    for (int32 i = 0; i < NumPoints - 1; ++i)
    {
        USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(this);
        if (!SplineMesh) continue;

        SplineMesh->SetMobility(EComponentMobility::Movable);
        SplineMesh->CreationMethod = EComponentCreationMethod::UserConstructionScript;
        SplineMesh->SetupAttachment(Spline);
        SplineMesh->SetStaticMesh(SegmentMesh);
        SplineMesh->SetForwardAxis(ESplineMeshAxis::Z, false);
        SplineMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        SplineMesh->SetGenerateOverlapEvents(true);
        SplineMesh->SetCollisionObjectType(ECC_GameTraceChannel2);
        SplineMesh->SetCollisionResponseToAllChannels(ECR_Overlap);
        SplineMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        SplineMesh->SetRenderCustomDepth(true);
        SplineMesh->SetCustomDepthStencilValue(0);
        SplineMesh->RegisterComponent();

        SegmentMeshes.Add(SplineMesh);

        const FVector StartPos = Spline->GetLocationAtSplinePoint(i,     ESplineCoordinateSpace::Local);
        const FVector StartTan = Spline->GetTangentAtSplinePoint(i,      ESplineCoordinateSpace::Local);
        const FVector EndPos   = Spline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::Local);
        const FVector EndTan   = Spline->GetTangentAtSplinePoint(i + 1,  ESplineCoordinateSpace::Local);

        SplineMesh->SetStartAndEnd(StartPos, StartTan, EndPos, EndTan, true);
        SplineMesh->SetStartScale(SegmentScale);
        SplineMesh->SetEndScale(SegmentScale);

        USphereComponent* HeatSphere = NewObject<USphereComponent>(this);
        HeatSphere->SetMobility(EComponentMobility::Movable);
        HeatSphere->CreationMethod = EComponentCreationMethod::UserConstructionScript;
        HeatSphere->SetupAttachment(SplineMesh);
        HeatSphere->SetRelativeLocation(FVector::ZeroVector);
        HeatSphere->SetSphereRadius(HeatEmitRadius);
        HeatSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        HeatSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
        HeatSphere->SetGenerateOverlapEvents(true);
        HeatSphere->SetHiddenInGame(!bShowDebugShapes);
        HeatSphere->SetVisibility(bShowDebugShapes);
        HeatSphere->bDrawOnlyIfSelected = !bShowDebugShapes;
        HeatSphere->RegisterComponent();

        HeatSpheres.Add(HeatSphere);
    }

    // IceHeatZone (스플라인 중간)
    {
        const int32  MidIndex = (NumPoints - 1) / 2;
        const FVector MidLocal = Spline->GetLocationAtSplinePoint(MidIndex, ESplineCoordinateSpace::Local);

        IceHeatZone = NewObject<USphereComponent>(this);
        IceHeatZone->SetMobility(EComponentMobility::Movable);
        IceHeatZone->CreationMethod = EComponentCreationMethod::UserConstructionScript;
        IceHeatZone->SetupAttachment(Spline);
        IceHeatZone->SetRelativeLocation(MidLocal);
        IceHeatZone->SetSphereRadius(IceHeatZoneRadius);
        IceHeatZone->SetCollisionProfileName(TEXT("Trigger"));
        IceHeatZone->SetGenerateOverlapEvents(true);
        IceHeatZone->SetHiddenInGame(!bShowDebugShapes);
        IceHeatZone->SetVisibility(bShowDebugShapes);
        IceHeatZone->bDrawOnlyIfSelected = !bShowDebugShapes;
        IceHeatZone->ShapeColor = FColor::Red;
        IceHeatZone->OnComponentBeginOverlap.AddDynamic(this, &AWire::OnIceHeatZoneBeginOverlap);
        IceHeatZone->OnComponentEndOverlap.AddDynamic(this, &AWire::OnIceHeatZoneEndOverlap);
        IceHeatZone->RegisterComponent();
    }

    ApplyPower();
    ApplyDebugVisibility();
}

// ─────────────────────────────────────────────────────────────────────────────
// 전원 전파
// ─────────────────────────────────────────────────────────────────────────────

void AWire::PropagatePowerToConnected()
{
    for (AActor* Target : ConnectedActors)
        if (ATransformation_actor* Conductor = Cast<ATransformation_actor>(Target))
            Conductor->SetPowered(bPoweredFinal);

    for (AWire* W : ConnectedWires)
        if (W && !W->bPoweredBySource)
            W->SetPowered(bPoweredFinal);
}