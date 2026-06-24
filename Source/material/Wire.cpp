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
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

#include "Engine/EngineTypes.h"

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

    for (AActor* CA : ConnectedActors)
    {
        ATransformation_actor* C = Cast<ATransformation_actor>(CA);
        if (!C || !C->IsConductive()) continue;
        const float BR = C->GetBlockResistance();
        for (const TObjectPtr<AWire>& WPtr : C->GetConnectedWiresList())
        {
            AWire* W = WPtr.Get();
            if (W && W != this && !VoltageMap.Contains(W) && !OutWires.Contains(W))
            {
                OutWires.Add(W);
                OutBlockR.Add(BR);
            }
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

    for (AActor* CA : ConnectedActors)
    {
        ATransformation_actor* C = Cast<ATransformation_actor>(CA);
        if (!C || !C->IsConductive()) continue;
        const float BR = C->GetBlockResistance();
        for (const TObjectPtr<AWire>& WPtr : C->GetConnectedWiresList())
        {
            AWire* W = WPtr.Get();
            if (W && W != this && !Visited.Contains(W) && !Next.Contains(W))
            {
                Next.Add(W);
                BlockExtraR.Add(W, BR);
            }
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

void AWire::TriggerCircuitSolve()
{
    if (!bPoweredBySource) return;

    const float SourceV = (BatteryVoltage > 0.f) ? BatteryVoltage : DefaultVoltage;

    // --- Step 1: 직렬 저항 계산 (옴의 법칙으로 전체 전류 산출) ---
    TArray<AWire*> FirstNext;
    TArray<float>  FirstBlockR;
    {
        TMap<AWire*, float> Dummy;
        CollectNextWiresWithBlockR(FirstNext, FirstBlockR, Dummy);
    }

    float TotalR = Resistance;
    if (FirstNext.Num() == 1)
    {
        TSet<AWire*> Rv; Rv.Add(this);
        TotalR = Resistance + FirstBlockR[0] + FirstNext[0]->CalcSeriesResistance(Rv);
    }
    else if (FirstNext.Num() > 1)
    {
        float InvR = 0.f;
        for (int32 i = 0; i < FirstNext.Num(); ++i)
        {
            TSet<AWire*> Rv; Rv.Add(this);
            const float BR = FirstBlockR[i] + FirstNext[i]->CalcSeriesResistance(Rv);
            if (BR > 0.001f) InvR += 1.f / BR;
        }
        TotalR = Resistance + (InvR > 0.f ? 1.f / InvR : 0.f);
    }
    const float SourceI = (TotalR > 0.001f) ? SourceV / TotalR : 0.f;

    // 배터리 소스 자신 값 설정
    EffectiveVoltage   = SourceV;
    EffectiveCurrent   = SourceI;
    bCircuitSolved     = true;
    if (UWorld* W = GetWorld()) LastSolveTimeSeconds = W->GetTimeSeconds();

    // --- Step 2: BFS 위상 정렬로 전파 ---
    // 각 전선의 상류 입력 개수를 먼저 셈
    TMap<AWire*, int32> IncomingCountMap;
    {
        TSet<AWire*> Visited;
        BuildCircuitGraph(IncomingCountMap, Visited);
    }

    struct NodeState
    {
        float MaxV   = 0.f;  // 전압: 상류 중 max
        float TotalI = 0.f;  // 전류: 상류 합산 (KCL)
        int32 Recv   = 0;    // 받은 입력 수
    };
    TMap<AWire*, NodeState> States;

    // 배터리 직결 다음 전선 시드
    TArray<AWire*> Queue;
    auto Seed = [&](AWire* W, float V, float I)
    {
        if (!W) return;
        NodeState& S = States.FindOrAdd(W);
        S.MaxV   = FMath::Max(S.MaxV, V);
        S.TotalI += I;
        S.Recv++;
        const int32 Exp = FMath::Max(IncomingCountMap.FindRef(W), 1);
        if (S.Recv >= Exp) Queue.AddUnique(W);
    };

    for (int32 i = 0; i < FirstNext.Num(); ++i)
    {
        const float VDrop = SourceI * (Resistance + FirstBlockR[i]);
        Seed(FirstNext[i], FMath::Max(SourceV - VDrop, 0.f), SourceI);
    }
    // 배터리에 직결된 블럭
    for (AActor* CA : ConnectedActors)
    {
        ATransformation_actor* C = Cast<ATransformation_actor>(CA);
        if (!C || !C->IsConductive()) continue;
        C->ClearPower(); C->ReceivePower(SourceV, SourceI);
        const float BR = C->GetBlockResistance();
        for (const TObjectPtr<AWire>& WPtr : C->GetConnectedWiresList())
        {
            const float VDrop = SourceI * (Resistance + BR);
            Seed(WPtr.Get(), FMath::Max(SourceV - VDrop, 0.f), SourceI);
        }
    }

    // BFS 처리
    int32 QIdx = 0;
    while (QIdx < Queue.Num())
    {
        AWire* Cur = Queue[QIdx++];
        NodeState& S = States[Cur];

        Cur->SetPowered(true);
        Cur->EffectiveVoltage = S.MaxV;
        Cur->bIsMergeNode     = (S.Recv >= 2);

        float OutI = S.TotalI;
        if (Cur->bIsParallel && Cur->ParallelBranchCount > 1)
            OutI = S.TotalI / float(Cur->ParallelBranchCount);
        Cur->EffectiveCurrent = OutI;

        if (Cur->bIsParallel && Cur->ParallelBranchCount > 1)
        {
            Cur->CachedCircuitText  = FString::Printf(TEXT("[병렬가지 1/%d] V:%.2f I:%.2fA"), Cur->ParallelBranchCount, S.MaxV, OutI);
            Cur->CachedCircuitColor = FColor::Green;
        }
        else if (Cur->bIsMergeNode)
        {
            Cur->CachedCircuitText  = FString::Printf(TEXT("[합류] V:%.2f I:%.2fA"), S.MaxV, OutI);
            Cur->CachedCircuitColor = FColor::Orange;
        }
        else
        {
            Cur->CachedCircuitText  = FString::Printf(TEXT("[직렬] V:%.2f I:%.2fA"), S.MaxV, OutI);
            Cur->CachedCircuitColor = FColor::Cyan;
        }

        Cur->bCircuitSolved = true;
        if (UWorld* W = GetWorld()) Cur->LastSolveTimeSeconds = W->GetTimeSeconds();

        // 블럭에 전력 전달
        for (AActor* CA : Cur->ConnectedActors)
        {
            ATransformation_actor* C = Cast<ATransformation_actor>(CA);
            if (!C) continue;
            C->ClearPower(); C->ReceivePower(S.MaxV, OutI);
        }

        // 다운스트림 전선 시드
        TArray<AWire*> NextWires; TArray<float> NextBlockR;
        {
            TMap<AWire*, float> Dummy;
            Cur->CollectNextWiresWithBlockR(NextWires, NextBlockR, Dummy);
        }
        for (int32 i = 0; i < NextWires.Num(); ++i)
        {
            const float VDrop = OutI * (Cur->Resistance + NextBlockR[i]);
            Seed(NextWires[i], FMath::Max(S.MaxV - VDrop, 0.f), OutI);
        }
        // 블럭 경유 다음 전선
        for (AActor* CA : Cur->ConnectedActors)
        {
            ATransformation_actor* C = Cast<ATransformation_actor>(CA);
            if (!C || !C->IsConductive()) continue;
            const float BR = C->GetBlockResistance();
            for (const TObjectPtr<AWire>& WPtr : C->GetConnectedWiresList())
            {
                const float VDrop = OutI * (Cur->Resistance + BR);
                Seed(WPtr.Get(), FMath::Max(S.MaxV - VDrop, 0.f), OutI);
            }
        }
    }
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
    // 저항이 0이면 이상 도체 → 발열 없음
    if (bPoweredFinal && EffectiveCurrent > 0.f && Resistance > 0.f)
    {
        CurrentAmps = EffectiveCurrent;

        const float EnergyJ = CurrentAmps * CurrentAmps * Resistance * DeltaTime * FMath::Max(SimTimeScale, 0.f);
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

    // 블럭은 END sphere로 자동 감지
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
                if (ATransformation_actor* Block = Cast<ATransformation_actor>(A))
                    ConnectedActors.AddUnique(Block);
        }
    }

    TriggerCircuitSolve();

    for (AActor* Target : ConnectedActors)
        if (ATransformation_actor* C = Cast<ATransformation_actor>(Target))
        {
            C->ClearPower();
            C->ReceivePower(EffectiveVoltage, EffectiveCurrent);
        }
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

    // 블럭은 END sphere로 자동 감지
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
            {
                if (ATransformation_actor* Block = Cast<ATransformation_actor>(A))
                    ConnectedActors.AddUnique(Block);
            }
        }
    }

    for (AActor* Target : ConnectedActors)
        if (ATransformation_actor* C = Cast<ATransformation_actor>(Target))
        {
            C->ClearPower();
            C->ReceivePower(EffectiveVoltage, EffectiveCurrent);
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
    const bool bHasActualPower = bPoweredFinal && EffectiveVoltage > 0.f;

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