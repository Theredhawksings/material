#include "Wire.h"
#include "Transformation_actor.h"
#include "Resistance.h"
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
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

#include "Engine/EngineTypes.h"
#include "EngineUtils.h"   // TActorIterator

AWire::AWire()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
    Spline->SetupAttachment(Root);

    ConnectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ConnectionSphere"));
    ConnectionSphere->SetupAttachment(Root);
    ConnectionSphere->SetSphereRadius(OverlapRadius);
    ConnectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ConnectionSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
    ConnectionSphere->SetCollisionObjectType(ECC_GameTraceChannel2);

    ConnectionSphereEnd = CreateDefaultSubobject<USphereComponent>(TEXT("ConnectionSphereEnd"));
    ConnectionSphereEnd->SetupAttachment(Root);
    ConnectionSphereEnd->SetSphereRadius(OverlapRadius);
    ConnectionSphereEnd->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ConnectionSphereEnd->SetCollisionResponseToAllChannels(ECR_Overlap);
    ConnectionSphereEnd->SetCollisionObjectType(ECC_GameTraceChannel2);

    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SparkFX(
    TEXT("/Game/modeling/Effect/NS_WireSparks.NS_WireSparks"));
if (SparkFX.Succeeded())
    SparkEffect = SparkFX.Object;
}


void AWire::BeginPlay()
{
    Super::BeginPlay();

    if (WireMaterial == EWireMaterial::Copper)    Resistance = 1.f;
    else if (WireMaterial == EWireMaterial::Iron) Resistance = 3.f;

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

#if ENABLE_DRAW_DEBUG
    if (!GetWorld()) return;

    FVector DebugPos = GetActorLocation() + FVector(0.f, 0.f, 80.f);
    if (Spline && Spline->GetNumberOfSplinePoints() >= 2)
    {
        const int32 MidIdx = (Spline->GetNumberOfSplinePoints() - 1) / 2;
        DebugPos = Spline->GetLocationAtSplinePoint(MidIdx, ESplineCoordinateSpace::World)
                 + FVector(0.f, 0.f, 80.f);
    }

    if (bDebugWire && WireTemperatureC > AmbientTemperatureC + 1.f)
    {
        const FColor TempColor = (WireTemperatureC > 400.f) ? FColor::Red
                                : (WireTemperatureC > 200.f) ? FColor::Orange
                                : FColor::Yellow;
        DrawDebugString(GetWorld(), DebugPos,
            FString::Printf(TEXT("%.0fC  %.2fA  V:%.2f"),
                WireTemperatureC, CurrentAmps, EffectiveVoltage),
            nullptr, TempColor, 0.0f, true);
    }

    if (bDebugCircuit && !CachedCircuitText.IsEmpty())
    {
        DrawDebugString(GetWorld(), DebugPos + FVector(0.f, 0.f, 25.f),
            CachedCircuitText, nullptr, CachedCircuitColor, 0.0f, true);
    }
#endif
}

void AWire::CollectNextWires(TArray<AWire*>& Out, const TMap<AWire*, float>& VoltageMap) const
{
    for (AWire* W : ConnectedWires)
        if (W && !VoltageMap.Contains(W))
            Out.AddUnique(W);

    for (AActor* CA : ConnectedActors)
    {
        ATransformation_actor* C = Cast<ATransformation_actor>(CA);
        if (!C || !C->IsConductive()) continue;
        for (const TObjectPtr<AWire>& WPtr : C->GetConnectedWiresList())
        {
            AWire* W = WPtr.Get();
            if (W && W != this && !VoltageMap.Contains(W))
                Out.AddUnique(W);
        }
    }
}

void AWire::CollectNextWiresWithBlockR(TArray<AWire*>& OutWires, TArray<float>& OutBlockR, const TMap<AWire*, float>& VoltageMap) const
{
    for (AWire* W : ConnectedWires)
    {
        if (W && !VoltageMap.Contains(W) && !OutWires.Contains(W))
        {
            OutWires.Add(W);
            OutBlockR.Add(0.f);
        }
    }

    auto AddBlockWires = [&](float BR, const TArray<TObjectPtr<AWire>>& WireList)
    {
        for (const TObjectPtr<AWire>& WPtr : WireList)
        {
            AWire* W = WPtr.Get();
            if (W && W != this && !VoltageMap.Contains(W) && !OutWires.Contains(W))
            {
                OutWires.Add(W);
                OutBlockR.Add(BR);
            }
        }
    };

    for (AActor* CA : ConnectedActors)
    {
        if (ATransformation_actor* C = Cast<ATransformation_actor>(CA))
        {
            if (C->IsConductive())
                AddBlockWires(C->GetBlockResistance(), C->GetConnectedWiresList());
        }
        else if (AResistance* R = Cast<AResistance>(CA))
        {
            AddBlockWires(R->GetBlockResistance(), R->GetConnectedWiresList());
        }
    }
}

float AWire::CalcSeriesResistance(TSet<AWire*>& Visited) const
{
    float Total = Resistance;

    TArray<AWire*> Next;
    TMap<AWire*, float> BlockExtraR;   // 블럭 경유 시 추가되는 블럭 저항

    for (AWire* W : ConnectedWires)
        if (W && !Visited.Contains(W))
            Next.AddUnique(W);

    auto AddBlockToNext = [&](float BR, const TArray<TObjectPtr<AWire>>& WireList)
    {
        for (const TObjectPtr<AWire>& WPtr : WireList)
        {
            AWire* W = WPtr.Get();
            if (W && W != this && !Visited.Contains(W) && !Next.Contains(W))
            {
                Next.Add(W);
                BlockExtraR.Add(W, BR);
            }
        }
    };

    for (AActor* CA : ConnectedActors)
    {
        if (ATransformation_actor* C = Cast<ATransformation_actor>(CA))
        {
            if (C->IsConductive())
                AddBlockToNext(C->GetBlockResistance(), C->GetConnectedWiresList());
        }
        else if (AResistance* R = Cast<AResistance>(CA))
        {
            AddBlockToNext(R->GetBlockResistance(), R->GetConnectedWiresList());
        }
    }

    if (Next.Num() == 1)
    {
        Visited.Add(Next[0]);
        const float ExtraR = BlockExtraR.FindRef(Next[0]);
        Total += ExtraR + Next[0]->CalcSeriesResistance(Visited);
    }
    else if (Next.Num() > 1)
    {
        // 병렬 합성: 1/R_total = 1/R1 + 1/R2 + ...
        float InvRSum = 0.f;
        for (AWire* N : Next)
        {
            TSet<AWire*> BranchVisited = Visited;
            BranchVisited.Add(N);
            const float ExtraR  = BlockExtraR.FindRef(N);
            const float BranchR = ExtraR + N->CalcSeriesResistance(BranchVisited);
            // BranchR=0인 가지(순수 전선만)는 단락 → 합성저항 0이므로 계산 스킵
            if (BranchR > 0.001f)
                InvRSum += 1.f / BranchR;
        }
        if (InvRSum > 0.f)
            Total += 1.f / InvRSum;
    }

    return Total;
}

void AWire::BuildCircuitGraph(TMap<AWire*, int32>& IncomingCountMap, TSet<AWire*>& Visited)
{
    if (Visited.Contains(this)) return;
    Visited.Add(this);

    for (AWire* Next : ConnectedWires)
    {
        if (!Next) continue;
        IncomingCountMap.FindOrAdd(Next, 0)++;
        if (!Visited.Contains(Next))
            Next->BuildCircuitGraph(IncomingCountMap, Visited);
    }

    for (AActor* CA : ConnectedActors)
    {
        ATransformation_actor* C = Cast<ATransformation_actor>(CA);
        if (!C || !C->IsConductive()) continue;
        for (const TObjectPtr<AWire>& WPtr : C->GetConnectedWiresList())
        {
            AWire* Next = WPtr.Get();
            if (!Next || Next == this) continue;
            IncomingCountMap.FindOrAdd(Next, 0)++;
            if (!Visited.Contains(Next))
                Next->BuildCircuitGraph(IncomingCountMap, Visited);
        }
    }
}

void AWire::PropagateVoltage(float IncomingVoltage, float IncomingCurrent,
                              TMap<AWire*, float>& VoltageMap,
                              TMap<AWire*, float>& CurrentAccumMap,
                              const TMap<AWire*, int32>& IncomingCountMap)
{
    // TriggerCircuitSolve의 BFS가 대신 처리 — 이 함수는 더 이상 재귀 호출되지 않음
}


void AWire::ResetVoltageNetwork(TSet<AWire*>& Visited)
{
    if (Visited.Contains(this)) return;
    Visited.Add(this);

    EffectiveVoltage     = 0.f;
    EffectiveCurrent     = 0.f;
    CachedCircuitText    = TEXT("");
    bCircuitSolved       = false;
    LastSolveTimeSeconds = -999.f;

    // ★ 전원 상태도 같이 리셋
    bPoweredBySource = false;
    bPoweredByMetal  = false;
    if (bPoweredFinal)
    {
        bPoweredFinal = false;
        ApplyPower();
    }

    for (AWire* W : ConnectedWires)
        if (W && !Visited.Contains(W))
            W->ResetVoltageNetwork(Visited);
}

// 저항 블럭 한 개를 일반화: 저항값 + 연결된 전선 목록
struct FBlockBridge
{
    AActor* Block = nullptr;
    float   R     = 0.f;
    TArray<AWire*> Wires;
};

void AWire::TriggerCircuitSolve()
{
    // 직전에 이 배터리가 켰던 전선 전부 끄기 (배터리 OFF/분리 시 사용)
    auto PowerDownPrev = [&]()
    {
        for (const TWeakObjectPtr<AWire>& Prev : PrevSolvedWires)
        {
            if (AWire* W = Prev.Get())
            {
                W->SetPowered(false);
                W->EffectiveVoltage = 0.f; W->EffectiveCurrent = 0.f;
                W->bCircuitSolved = false; W->LastSolveTimeSeconds = -999.f;
                W->CachedCircuitText = TEXT("");
                W->ApplyPower();
            }
        }
        PrevSolvedWires.Reset();
    };

    if (!bPoweredBySource) { PowerDownPrev(); return; }

    UWorld* World = GetWorld();
    if (!World) return;

    const float EMF = (BatteryVoltage > 0.f) ? BatteryVoltage : DefaultVoltage;

    // ============================================================
    //  1) 모든 전선/저항블럭 수집 + Union-Find 노드 구성
    // ============================================================
    TArray<AWire*> AllWires;
    for (TActorIterator<AWire> It(World); It; ++It)
        AllWires.Add(*It);

    // 저항 블럭 수집 (AResistance + 전도성 Transformation_actor)
    // 배터리 본체(this)는 노드가 아니라 전원이므로 블럭 전선 목록에서 제외
    TArray<FBlockBridge> Bridges;
    for (TActorIterator<AResistance> It(World); It; ++It)
    {
        FBlockBridge B; B.Block = *It; B.R = (*It)->GetBlockResistance();
        for (const TObjectPtr<AWire>& WPtr : (*It)->GetConnectedWiresList())
            if (WPtr && WPtr.Get() != this) B.Wires.AddUnique(WPtr.Get());
        if (B.Wires.Num() > 0) Bridges.Add(B);
    }
    for (TActorIterator<ATransformation_actor> It(World); It; ++It)
    {
        if (!It->IsConductive()) continue;
        FBlockBridge B; B.Block = *It; B.R = It->GetBlockResistance();
        for (const TObjectPtr<AWire>& WPtr : It->GetConnectedWiresList())
            if (WPtr && WPtr.Get() != this) B.Wires.AddUnique(WPtr.Get());
        if (B.Wires.Num() > 0) Bridges.Add(B);
    }

    // Union-Find (전선 단위, 직결 wire-wire 만 병합)
    TMap<AWire*, AWire*> Parent;
    for (AWire* W : AllWires) Parent.Add(W, W);
    TFunction<AWire*(AWire*)> Find = [&](AWire* X) -> AWire*
    {
        if (!Parent.Contains(X)) Parent.Add(X, X);   // 누락 전선 안전 처리
        while (Parent[X] != X) { Parent[X] = Parent[Parent[X]]; X = Parent[X]; }
        return X;
    };
    auto Union = [&](AWire* A, AWire* B)
    {
        if (!A || !B) return;
        AWire* ra = Find(A); AWire* rb = Find(B);
        if (ra != rb) Parent[ra] = rb;
    };

    // 직결 전선 병합 (배터리는 제외 — 배터리는 두 단자로 분리)
    for (AWire* A : AllWires)
    {
        if (A == this) continue;                 // 배터리 본체 제외
        for (const TObjectPtr<AWire>& DPtr : A->ManualDownstreamWires)
        {
            AWire* B = DPtr.Get();
            if (B && B != this) Union(A, B);      // 일반 전선끼리만 병합
        }
    }

    // 배터리 + 단자(P): 배터리의 다운스트림 전선들
    TArray<AWire*> Pwires;
    for (const TObjectPtr<AWire>& DPtr : ManualDownstreamWires)
        if (DPtr) Pwires.Add(DPtr.Get());
    for (int32 i = 1; i < Pwires.Num(); ++i) Union(Pwires[0], Pwires[i]);

    // 배터리 − 단자(N): 귀환 전선들
    //  (1) 명시 지정: 배터리의 ManualReturnWires  ← 가장 확실
    //  (2) 수동: 배터리를 ManualDownstreamWires로 가리키는 전선
    //  (3) 자동: 배터리 출력 전선의 START(뒤쪽)에 끝점이 닿은 전선
    TArray<AWire*> Nwires;

    // (1) 배터리에 직접 지정한 귀환 전선
    for (const TObjectPtr<AWire>& RPtr : ManualReturnWires)
        if (RPtr && RPtr.Get() != this && !Pwires.Contains(RPtr.Get()))
            Nwires.AddUnique(RPtr.Get());

    const FVector MyStart = GetStartPointLocation();
    const float   ReturnThreshold = OverlapRadius * 2.f;
    for (AWire* A : AllWires)
    {
        if (A == this || Nwires.Contains(A)) continue;
        if (Pwires.Contains(A)) continue;   // 출발 전선은 귀환이 될 수 없음
        bool bIsReturn = false;

        // (2) A가 배터리를 다운스트림으로 가리킴
        for (const TObjectPtr<AWire>& DPtr : A->ManualDownstreamWires)
            if (DPtr.Get() == this) { bIsReturn = true; break; }

        // (3) 자동 기하 감지: A의 끝점이 배터리 START에 닿음
        if (!bIsReturn)
        {
            const float dEnd   = FVector::Dist(A->GetEndPointLocation(),   MyStart);
            const float dStart = FVector::Dist(A->GetStartPointLocation(), MyStart);
            if (FMath::Min(dEnd, dStart) <= ReturnThreshold)
                bIsReturn = true;
        }

        if (bIsReturn) Nwires.AddUnique(A);
    }
    for (int32 i = 1; i < Nwires.Num(); ++i) Union(Nwires[0], Nwires[i]);

    // 진단 로그: 솔버가 인식한 출발/귀환 전선 수
    UE_LOG(LogTemp, Warning, TEXT("[배터리 %s] 출발(P):%d개  귀환(N):%d개  저항블럭:%d개"),
        *GetName(), Pwires.Num(), Nwires.Num(), Bridges.Num());

    // 배터리에서 나가는 전선이 아예 없으면 할 게 없음
    if (Pwires.Num() == 0)
    {
        PowerDownPrev();
        EffectiveVoltage = EMF; EffectiveCurrent = 0.f;
        bCircuitSolved = true;
        LastSolveTimeSeconds = World->GetTimeSeconds();
        CachedCircuitText  = FString::Printf(TEXT("[배터리 %.1fV] 출발 전선 없음!"), EMF);
        CachedCircuitColor = FColor::Red;
        return;
    }

    const bool bHasReturn = (Nwires.Num() > 0);   // 귀환 경로(폐회로) 존재 여부

    AWire* Proot = Find(Pwires[0]);
    AWire* Nroot = bHasReturn ? Find(Nwires[0]) : nullptr;

    // ============================================================
    //  2) 노드 인덱스 + 저항 엣지 구성
    // ============================================================
    // 노드 = Union-Find 루트. P/N 은 고정 전압.
    TMap<AWire*, int32> NodeIdx;
    auto NodeOf = [&](AWire* W) -> int32
    {
        AWire* r = Find(W);
        if (int32* Found = NodeIdx.Find(r)) return *Found;
        int32 NewIdx = NodeIdx.Num();
        NodeIdx.Add(r, NewIdx);
        return NewIdx;
    };
    const int32 PIdx = NodeOf(Pwires[0]);
    const int32 NIdx = bHasReturn ? NodeOf(Nwires[0]) : -1;

    // 저항 엣지: (노드a, 노드b, R)
    struct FEdge { int32 A; int32 B; float R; AActor* Block; };
    TArray<FEdge> Edges;
    for (const FBlockBridge& Bg : Bridges)
    {
        if (Bg.R <= 0.0001f) continue;
        // 이 블럭이 잇는 서로 다른 노드들
        TArray<int32> DistinctNodes;
        for (AWire* W : Bg.Wires)
            if (W) DistinctNodes.AddUnique(NodeOf(W));
        // 2개 노드를 잇는 저항 (정상 케이스)
        if (DistinctNodes.Num() == 2)
            Edges.Add({ DistinctNodes[0], DistinctNodes[1], Bg.R, Bg.Block });
        else if (DistinctNodes.Num() > 2)
        {
            // 비정상: 첫 노드를 중심으로 별 연결 (드문 케이스 폴백)
            for (int32 k = 1; k < DistinctNodes.Num(); ++k)
                Edges.Add({ DistinctNodes[0], DistinctNodes[k], Bg.R, Bg.Block });
        }
    }

    const int32 NumNodes = NodeIdx.Num();

    // 노드 인접 리스트 (저항 엣지 기준)
    TArray<TArray<int32>> Adj; Adj.SetNum(NumNodes);
    for (const FEdge& E : Edges)
    {
        Adj[E.A].AddUnique(E.B);
        Adj[E.B].AddUnique(E.A);
    }

    // P에서 도달 가능한 노드 집합 (BFS)
    TArray<bool> Reach; Reach.Init(false, NumNodes);
    {
        TArray<int32> Q; Q.Add(PIdx); Reach[PIdx] = true;
        int32 qi = 0;
        while (qi < Q.Num())
        {
            const int32 c = Q[qi++];
            for (int32 nb : Adj[c])
                if (!Reach[nb]) { Reach[nb] = true; Q.Add(nb); }
        }
    }

    // 폐회로 = 귀환 경로 존재 && N이 P에서 저항망 통해 도달 가능
    const bool bClosedLoop = bHasReturn && (NIdx >= 0) && Reach[NIdx];

    TArray<float> NodeV; NodeV.Init(0.f, NumNodes);
    NodeV[PIdx] = EMF;
    if (NIdx >= 0) NodeV[NIdx] = 0.f;

    // ============================================================
    //  3) 노드 해석
    // ============================================================
    if (!bClosedLoop)
    {
        // 개방회로: 전류 0 → 전압강하 0 → P에 연결된 모든 노드 = EMF
        for (int32 n = 0; n < NumNodes; ++n)
            NodeV[n] = Reach[n] ? EMF : 0.f;
    }
    else
    {
    // 폐회로 → KCL 연립방정식 (가우스 소거). 고정: P=EMF, N=0.
    TArray<int32> UnknownNodes;
    TArray<int32> NodeToUnknown; NodeToUnknown.Init(-1, NumNodes);
    for (int32 n = 0; n < NumNodes; ++n)
        if (n != PIdx && n != NIdx)
        { NodeToUnknown[n] = UnknownNodes.Num(); UnknownNodes.Add(n); }

    const int32 U = UnknownNodes.Num();
    if (U > 0)
    {
        // G * x = b
        TArray<float> G; G.Init(0.f, U * U);
        TArray<float> b; b.Init(0.f, U);
        auto Gat = [&](int32 r, int32 c) -> float& { return G[r * U + c]; };

        for (const FEdge& E : Edges)
        {
            const float g = 1.f / E.R;
            const int32 ua = NodeToUnknown[E.A];
            const int32 ub = NodeToUnknown[E.B];
            // 노드 A의 KCL
            if (ua >= 0)
            {
                Gat(ua, ua) += g;
                if (ub >= 0) Gat(ua, ub) -= g;
                else         b[ua] += g * NodeV[E.B]; // B 고정
            }
            // 노드 B의 KCL
            if (ub >= 0)
            {
                Gat(ub, ub) += g;
                if (ua >= 0) Gat(ub, ua) -= g;
                else         b[ub] += g * NodeV[E.A]; // A 고정
            }
        }

        // 가우스 소거 (부분 피벗)
        for (int32 col = 0; col < U; ++col)
        {
            int32 piv = col;
            for (int32 r = col + 1; r < U; ++r)
                if (FMath::Abs(Gat(r, col)) > FMath::Abs(Gat(piv, col))) piv = r;
            if (FMath::Abs(Gat(piv, col)) < 1e-9f) continue; // 특이 → 스킵
            if (piv != col)
            {
                for (int32 c = 0; c < U; ++c) Swap(Gat(col, c), Gat(piv, c));
                Swap(b[col], b[piv]);
            }
            const float pivVal = Gat(col, col);
            for (int32 r = 0; r < U; ++r)
            {
                if (r == col) continue;
                const float f = Gat(r, col) / pivVal;
                if (f == 0.f) continue;
                for (int32 c = col; c < U; ++c) Gat(r, c) -= f * Gat(col, c);
                b[r] -= f * b[col];
            }
        }
        for (int32 r = 0; r < U; ++r)
        {
            const float d = Gat(r, r);
            NodeV[UnknownNodes[r]] = (FMath::Abs(d) > 1e-9f) ? b[r] / d : 0.f;
        }
    }
    } // end 폐회로

    // ============================================================
    //  4) 결과 반영: 노드별 전압 + 통과 전류
    // ============================================================
    // 노드별 통과 전류 = (Σ|엣지 전류|)/2
    TArray<float> NodeThroughI; NodeThroughI.Init(0.f, NumNodes);
    for (const FEdge& E : Edges)
    {
        const float I = (NodeV[E.A] - NodeV[E.B]) / E.R;
        NodeThroughI[E.A] += FMath::Abs(I);
        NodeThroughI[E.B] += FMath::Abs(I);
        // 블럭에 강하/전류 표시
        const float Drop = FMath::Abs(NodeV[E.A] - NodeV[E.B]);
        if (ATransformation_actor* C = Cast<ATransformation_actor>(E.Block))
        { C->ClearPower(); C->ReceivePower(Drop, FMath::Abs(I)); }
        else if (AResistance* R = Cast<AResistance>(E.Block))
        { R->ClearPower(); R->ReceivePower(Drop, FMath::Abs(I)); }
    }
    // 내부 노드는 in=out 이므로 Σ|I|/2 = 통과전류.
    // P/N(배터리 단자)는 한쪽에만 저항 엣지가 있어 /2 하면 안 됨.
    for (int32 n = 0; n < NumNodes; ++n)
        if (n != PIdx && n != NIdx) NodeThroughI[n] *= 0.5f;

    // 배터리 전류 = P 노드에서 나가는 전류 합
    float BatteryI = 0.f;
    for (const FEdge& E : Edges)
    {
        if (E.A == PIdx) BatteryI += (NodeV[PIdx] - NodeV[E.B]) / E.R;
        else if (E.B == PIdx) BatteryI += (NodeV[PIdx] - NodeV[E.A]) / E.R;
    }

    // 각 전선에 노드 값 반영 + 이번에 전원 준 전선 집합 수집
    const float Now = World->GetTimeSeconds();
    TSet<TWeakObjectPtr<AWire>> NowSolved;
    for (AWire* W : AllWires)
    {
        if (W == this) continue;
        AWire* r = Find(W);
        const int32* IdxPtr = NodeIdx.Find(r);
        if (!IdxPtr) continue;             // 이 회로에 속하지 않음
        const int32 nidx = *IdxPtr;
        if (!Reach[nidx]) continue;        // 배터리에서 도달 불가 → 전원 안 줌

        W->SetPowered(true);
        W->EffectiveVoltage = NodeV[nidx];
        W->EffectiveCurrent = NodeThroughI[nidx];
        W->bIsMergeNode     = false;
        W->bCircuitSolved   = true;
        W->LastSolveTimeSeconds = Now;
        // 발전기 소스면 하류 전선도 개방회로 발열 (일반 배터리면 false 로 초기화)
        W->bOpenCircuitHeating = bOpenCircuitHeating;
        W->CachedCircuitText  = FString::Printf(TEXT("[V:%.2f I:%.2fA]"), NodeV[nidx], NodeThroughI[nidx]);
        W->CachedCircuitColor = FColor::Cyan;
        NowSolved.Add(W);
    }

    // 직전엔 켰지만 이번엔 회로에서 빠진 전선 → 즉시 전원 차단
    for (const TWeakObjectPtr<AWire>& Prev : PrevSolvedWires)
    {
        AWire* W = Prev.Get();
        if (!W || NowSolved.Contains(W)) continue;
        W->SetPowered(false);
        W->EffectiveVoltage     = 0.f;
        W->EffectiveCurrent     = 0.f;
        W->bCircuitSolved       = false;
        W->LastSolveTimeSeconds = -999.f;
        W->CachedCircuitText    = TEXT("");
        W->ApplyPower();
    }
    PrevSolvedWires = MoveTemp(NowSolved);

    // 배터리 본체
    EffectiveVoltage     = EMF;
    EffectiveCurrent     = BatteryI;
    bCircuitSolved       = true;
    LastSolveTimeSeconds = Now;
    CachedCircuitText    = FString::Printf(TEXT("[배터리 %.1fV] 출발:%d 귀환:%d %s I:%.2fA"),
        EMF, Pwires.Num(), Nwires.Num(),
        bClosedLoop ? TEXT("폐회로") : TEXT("개방"), BatteryI);
    CachedCircuitColor   = bClosedLoop ? FColor::Red : FColor::Yellow;
}

void AWire::UpdateFinalPower()
{
    const bool bNewFinal = (bPoweredBySource || bPoweredByMetal);
    if (bPoweredFinal == bNewFinal) return;

    bPoweredFinal = bNewFinal;
    ApplyPower();
    PropagatePowerToConnected();

    if (!bPoweredFinal)
    {
        return;
    }
}

void AWire::SetPowered(bool bNewPowered, bool bStartIsInput)
{
    if (bPoweredBySource == bNewPowered) return;
    bPoweredBySource = bNewPowered;
    bInputIsStart    = bStartIsInput;
    bCircuitSolved   = false;  // ★ 추가
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

void AWire::UpdateJouleHeating(float DeltaTime)
{
    // 발열 계산은 전기적 Resistance(=0, 이상 도체)가 아니라
    // 발열 전용 HeatingResistance 를 사용 → 전류가 흐르면 열화상이 작동.
    const float HeatR = (Resistance > 0.f) ? Resistance : HeatingResistance;

    // 발전기/코일 전선: 폐회로가 아니어서 전류가 0이어도
    // 전압이 걸려 있으면 V/R 로 발열 전류 환산 (예전 방식 복원)
    float HeatI = EffectiveCurrent;
    if (HeatI <= 0.f && bOpenCircuitHeating && bPoweredFinal && EffectiveVoltage > 0.f)
        HeatI = EffectiveVoltage / FMath::Max(HeatR, 0.1f);

    if (bPoweredFinal && HeatI > 0.f && HeatR > 0.f)
    {
        CurrentAmps = HeatI;

        const float EnergyJ = CurrentAmps * CurrentAmps * HeatR * DeltaTime * FMath::Max(SimTimeScale, 0.f);
        WireTemperatureC += EnergyJ / FMath::Max(WireMassKg * SpecificHeatJPerKgK, 0.01f);
    }
    else
    {
        CurrentAmps = 0.f;
    }

    // ★ 합류 전선: 기준(전력 큰) 상류 온도까지 빠르게 따라붙어 끊김 없게
    if (bPoweredFinal && bIsMergeNode && HeatFollowTargetC > WireTemperatureC)
    {
        const float Alpha = FMath::Clamp(MergeFollowRate * DeltaTime, 0.f, 1.f);
        WireTemperatureC += (HeatFollowTargetC - WireTemperatureC) * Alpha;
    }

    if (WireTemperatureC > AmbientTemperatureC)   // ← 기존 냉각 블록
    {
        const float Cool = CoolingRateKPerSec * ((WireTemperatureC - AmbientTemperatureC) / 100.f) * DeltaTime;
        WireTemperatureC = FMath::Max(WireTemperatureC - Cool, AmbientTemperatureC);
    }

    WireTemperatureC = FMath::Clamp(WireTemperatureC, AmbientTemperatureC, MaxWireTemperatureC);
    UpdateWireVisual();
}

void AWire::RefreshConnectedActors()
{
    TArray<TObjectPtr<AActor>> PrevConnectedActors = ConnectedActors;
    ConnectedActors.Reset();
    ConnectedWires.Reset();

    bIsMergeNode      = false;
    HeatFollowTargetC = -1.f;

    // 업스트림은 PropagateVoltage가 처리 → 폴링은 타임아웃만 체크
    ATransformation_actor* UpstreamBlock = nullptr;

    if (ConnectionSphere)
    {
        TArray<UPrimitiveComponent*> OverlappingComps;
        ConnectionSphere->GetOverlappingComponents(OverlappingComps);
        for (UPrimitiveComponent* Comp : OverlappingComps)
        {
            if (!Comp) continue;
            AActor* A = Comp->GetOwner();
            if (!A || A == this) continue;
            if (A->ActorHasTag(FName("Metal")) || A->ActorHasTag(FName("Copper")))
            {
                if (SourceWire != nullptr) continue;
                ATransformation_actor* Block = Cast<ATransformation_actor>(A);
                if (Block && !UpstreamBlock && Block->IsElectrified() && Block->GetEffectiveVoltage() > 0.f)
                    UpstreamBlock = Block;
            }
        }
    }

    // 비-배터리 전선: PropagateVoltage가 일정 시간 안 오면 무조건 리셋
    // (블럭/전선 제거 후에도 bPoweredBySource가 stale로 남는 문제 방지)
    if (!bIsBatterySource)
    {
        const float Now     = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
        const float Timeout = RefreshInterval * 2.5f;
        if ((Now - LastSolveTimeSeconds) > Timeout)
        {
            bPoweredBySource     = false;
            bCircuitSolved       = false;
            EffectiveVoltage     = 0.f;
            EffectiveCurrent     = 0.f;
            LastSolveTimeSeconds = -999.f;
        }
    }

    bool bFoundPower = false;
    if (bIsBatterySource)
        bFoundPower = bPoweredBySource;
    else if (bPoweredBySource)
        bFoundPower = true;
    else if (UpstreamBlock && UpstreamBlock->GetEffectiveVoltage() > 0.f)
        bFoundPower = true;

    SetPoweredByMetal(bFoundPower);

if (bIsBatterySource && bPoweredFinal)
{
    // 수동 지정 다운스트림 전선 사용
    for (TObjectPtr<AWire>& W : ManualDownstreamWires)
        if (W) ConnectedWires.AddUnique(W.Get());

    // 블럭은 END sphere로 자동 감지 (Transformation_actor + Resistance 둘 다)
    if (ConnectionSphereEnd)
    {
        TArray<UPrimitiveComponent*> OverlappingComps;
        ConnectionSphereEnd->GetOverlappingComponents(OverlappingComps);
        for (UPrimitiveComponent* Comp : OverlappingComps)
        {
            if (!Comp) continue;
            AActor* A = Comp->GetOwner();
            if (!A || A == this) continue;
            if (A->ActorHasTag(FName("Metal")) || A->ActorHasTag(FName("Copper")))
                ConnectedActors.AddUnique(A);
        }
    }

    // 노드 해석 솔버가 모든 전선·블럭 값을 직접 계산/반영함
    TriggerCircuitSolve();
    ApplyPower();
    return;
}

    if (!bPoweredFinal)
    {
        EffectiveVoltage     = 0.f;
        EffectiveCurrent     = 0.f;
        bCircuitSolved       = false;
        LastSolveTimeSeconds = -999.f;
        ApplyPower();
        for (AActor* Target : PrevConnectedActors)
            if (ATransformation_actor* C = Cast<ATransformation_actor>(Target))
                C->ClearPower();
        // ★ 전원 꺼지면 스파크 제거
    if (SparkComponentStart) { SparkComponentStart->DestroyComponent(); SparkComponentStart = nullptr; }
    if (SparkComponentEnd)   { SparkComponentEnd->DestroyComponent();   SparkComponentEnd = nullptr; }

    
        return;
    }

// PropagateVoltage가 이미 정확히 계산해서 bCircuitSolved를 세팅했으면
// 폴링이 덮어쓰지 않음 → 값 안정화
if (!bCircuitSolved)
{
    // 블럭에서 전원 받는 경우만 폴링으로 V/I 설정
    if (UpstreamBlock && UpstreamBlock->GetEffectiveVoltage() > 0.f)
    {
        EffectiveVoltage = UpstreamBlock->GetEffectiveVoltage();
        EffectiveCurrent = UpstreamBlock->GetEffectiveCurrent();
        CachedCircuitText  = FString::Printf(TEXT("[직렬] V:%.2f I:%.2fA"), EffectiveVoltage, EffectiveCurrent);
        CachedCircuitColor = FColor::Cyan;
    }
}

    // 수동 지정 다운스트림 전선 사용
    for (TObjectPtr<AWire>& W : ManualDownstreamWires)
        if (W) ConnectedWires.AddUnique(W.Get());

    // 블럭은 END sphere로 자동 감지 (Transformation_actor + Resistance 둘 다)
    if (ConnectionSphereEnd)
    {
        TArray<UPrimitiveComponent*> OverlappingComps;
        ConnectionSphereEnd->GetOverlappingComponents(OverlappingComps);
        for (UPrimitiveComponent* Comp : OverlappingComps)
        {
            if (!Comp) continue;
            AActor* A = Comp->GetOwner();
            if (!A || A == this) continue;
            if (A->ActorHasTag(FName("Metal")) || A->ActorHasTag(FName("Copper")))
                ConnectedActors.AddUnique(A);
        }
    }

        // ★ 전선-전선 연결 시 스파크
    // 전압과 전류가 모두 0 초과일 때만 스파크 표시
    const bool bHasRealPower = bPoweredFinal && EffectiveVoltage > 0.f && EffectiveCurrent > 0.f;
    if (SparkEffect && bHasRealPower)
    {
        // START: 내 앞에 upstream이 있을 때 (bPoweredBySource = 배터리에서 propagate 받음)
        if (bPoweredBySource && !bIsBatterySource && !SparkComponentStart)
            SparkComponentStart = UNiagaraFunctionLibrary::SpawnSystemAttached(
                SparkEffect, ConnectionSphere, NAME_None,
                FVector::ZeroVector, FRotator::ZeroRotator,
                EAttachLocation::SnapToTarget, true);
        else if ((!bPoweredBySource || bIsBatterySource) && SparkComponentStart)
        { SparkComponentStart->DestroyComponent(); SparkComponentStart = nullptr; }

        if (ConnectedWires.Num() > 0 && !SparkComponentEnd)
            SparkComponentEnd = UNiagaraFunctionLibrary::SpawnSystemAttached(
                SparkEffect, ConnectionSphereEnd, NAME_None,
                FVector::ZeroVector, FRotator::ZeroRotator,
                EAttachLocation::SnapToTarget, true);
        else if (ConnectedWires.Num() == 0 && SparkComponentEnd)
        { SparkComponentEnd->DestroyComponent(); SparkComponentEnd = nullptr; }
    }
    else
    {
        if (SparkComponentStart) { SparkComponentStart->DestroyComponent(); SparkComponentStart = nullptr; }
        if (SparkComponentEnd)   { SparkComponentEnd->DestroyComponent();   SparkComponentEnd = nullptr; }
    }

    ApplyPower();
}


void AWire::EmitHeatToNearby(float DeltaTime)
{
    if (!GetWorld()) return;

    const float T_K        = WireTemperatureC + 273.15f;
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
        GetWorld()->OverlapMultiByObjectType(Hits, Center, FQuat::Identity, ObjParams,
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

void AWire::OnIceHeatZoneBeginOverlap(UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32, bool, const FHitResult&) {}
void AWire::OnIceHeatZoneEndOverlap(UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32) {}

void AWire::UpdateConnectionPoint()
{
    if (!Spline) return;
    const int32 NumPoints = Spline->GetNumberOfSplinePoints();
    if (NumPoints < 1) return;

    if (ConnectionSphere)
    {
        ConnectionSphere->SetRelativeLocation(
            Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local));
        ConnectionSphere->SetSphereRadius(OverlapRadius);
    }

    if (ConnectionSphereEnd)
    {
        ConnectionSphereEnd->SetRelativeLocation(
            Spline->GetLocationAtSplinePoint(NumPoints - 1, ESplineCoordinateSpace::Local));
        ConnectionSphereEnd->SetSphereRadius(OverlapRadius);
    }
}

void AWire::ApplyPower()
{
    // 전류가 흐르거나 전압이 있으면 점등 → 닫힌 고리 전체가 빛남
    // (귀환선=0V여도 전류 흐르면 켜짐 → 회로 완성이 한눈에 보임)
    const bool bHasActualPower = bPoweredFinal && (EffectiveVoltage > 0.f || EffectiveCurrent > 0.f);

    // ★ 상태 변화 없으면 스킵
    if (bHasActualPower == bLastAppliedPowerState && SegmentMIDs.Num() == SegmentMeshes.Num())
        return;

    if (bHasActualPower && OnMaterial)
    {
        SegmentMIDs.Reset();
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
        SegmentMIDs.Reset();
        for (USplineMeshComponent* Mesh : SegmentMeshes)
        {
            if (!IsValid(Mesh) || !OffMaterial) continue;
            Mesh->SetMaterial(0, OffMaterial);
        }
    }
    bLastAppliedPowerState = bHasActualPower;
}

void AWire::UpdateWireVisual()
{
    // ★ 전원 여부와 무관하게 항상 온도 기준으로 스텐실 계산
    const float TempRatio = FMath::Clamp(
        (WireTemperatureC - AmbientTemperatureC) / (MaxWireTemperatureC - AmbientTemperatureC),
        0.f, 1.f);
    const int32 StencilVal = FMath::RoundToInt(TempRatio * 255.f);
    const float Alpha = FMath::Clamp(WireTemperatureC * WireTempVisualScale, 0.f, 1.f);

    CachedWireHeatAlpha    = Alpha;
    CachedWireStencilValue = StencilVal;

    for (UMaterialInstanceDynamic* MID : SegmentMIDs)
        if (MID) MID->SetScalarParameterValue(WireHeatParamName, Alpha);

    for (USplineMeshComponent* Mesh : SegmentMeshes)
        if (Mesh)
        {
            Mesh->SetRenderCustomDepth(StencilVal > 0);   // 식으면 자동으로 꺼짐
            Mesh->SetCustomDepthStencilValue(StencilVal);
        }
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

#if WITH_EDITOR
void AWire::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    const FName PropName = PropertyChangedEvent.Property
        ? PropertyChangedEvent.Property->GetFName() : NAME_None;

    if (PropName == GET_MEMBER_NAME_CHECKED(AWire, bShowDebugShapes))
        ApplyDebugVisibility();
    if (PropName == GET_MEMBER_NAME_CHECKED(AWire, IceHeatZoneRadius))
        if (IceHeatZone) { IceHeatZone->SetSphereRadius(IceHeatZoneRadius); IceHeatZone->MarkRenderStateDirty(); }
}
#endif

void AWire::ClearGeneratedMeshes()
{
    for (USplineMeshComponent* Comp : SegmentMeshes)
        if (Comp) { Comp->UnregisterComponent(); Comp->DestroyComponent(); }
    SegmentMeshes.Empty();
    SegmentMIDs.Empty();

    for (USphereComponent* Sphere : HeatSpheres)
        if (Sphere) { Sphere->UnregisterComponent(); Sphere->DestroyComponent(); }
    HeatSpheres.Empty();

    if (IceHeatZone) { IceHeatZone->UnregisterComponent(); IceHeatZone->DestroyComponent(); IceHeatZone = nullptr; }
}

void AWire::RebuildSplineMeshes()
{
    ClearGeneratedMeshes();
    if (!Spline || !SegmentMesh) return;

    const int32 NumPoints = Spline->GetNumberOfSplinePoints();
    if (NumPoints < 2) return;

    for (int32 i = 0; i < NumPoints - 1; ++i)
    {
        USplineMeshComponent* SM = NewObject<USplineMeshComponent>(this);
        if (!SM) continue;

        SM->SetMobility(EComponentMobility::Movable);
        SM->CreationMethod = EComponentCreationMethod::UserConstructionScript;
        SM->SetupAttachment(Spline);
        SM->SetStaticMesh(SegmentMesh);
        SM->SetForwardAxis(ESplineMeshAxis::Z, false);
        SM->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        SM->SetGenerateOverlapEvents(true);
        SM->SetCollisionObjectType(ECC_GameTraceChannel2);
        SM->SetCollisionResponseToAllChannels(ECR_Overlap);
        SM->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        SM->SetRenderCustomDepth(true);
        SM->SetCustomDepthStencilValue(0);
        SM->RegisterComponent();
        SegmentMeshes.Add(SM);

        SM->SetStartAndEnd(
            Spline->GetLocationAtSplinePoint(i,     ESplineCoordinateSpace::Local),
            Spline->GetTangentAtSplinePoint(i,      ESplineCoordinateSpace::Local),
            Spline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::Local),
            Spline->GetTangentAtSplinePoint(i + 1,  ESplineCoordinateSpace::Local), true);
        SM->SetStartScale(SegmentScale);
        SM->SetEndScale(SegmentScale);

        USphereComponent* HS = NewObject<USphereComponent>(this);
        HS->SetMobility(EComponentMobility::Movable);
        HS->CreationMethod = EComponentCreationMethod::UserConstructionScript;
        HS->SetupAttachment(SM);
        HS->SetRelativeLocation(FVector::ZeroVector);
        HS->SetSphereRadius(HeatEmitRadius);
        HS->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        HS->SetCollisionResponseToAllChannels(ECR_Overlap);
        HS->SetGenerateOverlapEvents(true);
        HS->SetHiddenInGame(!bShowDebugShapes);
        HS->SetVisibility(bShowDebugShapes);
        HS->bDrawOnlyIfSelected = !bShowDebugShapes;
        HS->RegisterComponent();
        HeatSpheres.Add(HS);
    }

    {
        const int32   Mid    = (NumPoints - 1) / 2;
        const FVector MidLoc = Spline->GetLocationAtSplinePoint(Mid, ESplineCoordinateSpace::Local);

        IceHeatZone = NewObject<USphereComponent>(this);
        IceHeatZone->SetMobility(EComponentMobility::Movable);
        IceHeatZone->CreationMethod = EComponentCreationMethod::UserConstructionScript;
        IceHeatZone->SetupAttachment(Spline);
        IceHeatZone->SetRelativeLocation(MidLoc);
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
    UpdateConnectionPoint();
}

void AWire::PropagatePowerToConnected()
{
    for (AWire* W : ConnectedWires)
        if (W && !W->bPoweredBySource)
            W->SetPowered(bPoweredFinal);
}

FVector AWire::GetStartPointLocation() const
{
    return ConnectionSphere ? ConnectionSphere->GetComponentLocation() : GetActorLocation();
}

FVector AWire::GetEndPointLocation() const
{
    return ConnectionSphereEnd ? ConnectionSphereEnd->GetComponentLocation() : GetActorLocation();
}