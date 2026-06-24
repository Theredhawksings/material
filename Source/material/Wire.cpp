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
    if (VoltageMap.Contains(this)) return;
    VoltageMap.Add(this, IncomingVoltage);

    EffectiveVoltage = IncomingVoltage;

    // 병렬 가지 여부는 에디터에서 수동 설정한 bIsParallel 사용
    if (bIsParallel && ParallelBranchCount > 1)
    {
        EffectiveCurrent   = IncomingCurrent / float(ParallelBranchCount);
        CachedCircuitText  = FString::Printf(TEXT("[병렬가지 1/%d] V:%.2f I:%.2fA"),
            ParallelBranchCount, EffectiveVoltage, EffectiveCurrent);
        CachedCircuitColor = FColor::Green;
    }
    else
    {
        EffectiveCurrent   = IncomingCurrent;
        CachedCircuitText  = FString::Printf(TEXT("[직렬] V:%.2f I:%.2fA"),
            EffectiveVoltage, EffectiveCurrent);
        CachedCircuitColor = FColor::Cyan;
    }

    // 회로 해석 완료 표시 + 시각 기록
    bCircuitSolved       = true;
    if (UWorld* W = GetWorld())
        LastSolveTimeSeconds = W->GetTimeSeconds();

    TArray<AWire*> NextWires;
    TArray<float>  NextBlockR;
    CollectNextWiresWithBlockR(NextWires, NextBlockR, VoltageMap);

    if (NextWires.Num() == 0) return;

    for (int32 i = 0; i < NextWires.Num(); ++i)
    {
        AWire* Next = NextWires[i];
        // 전선 저항 + 블럭 경유 저항까지 합산해서 전압 강하 계산
        const float VDrop = EffectiveCurrent * (Resistance + NextBlockR[i]);
        const float NextV = FMath::Max(IncomingVoltage - VDrop, 0.f);
        Next->SetPowered(true);
        Next->PropagateVoltage(NextV, EffectiveCurrent, VoltageMap, CurrentAccumMap, IncomingCountMap);
    }
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

    TMap<AWire*, int32> IncomingCountMap;
    TSet<AWire*> GraphVisited;
    BuildCircuitGraph(IncomingCountMap, GraphVisited);

    TMap<AWire*, float> VoltageMap;
    TMap<AWire*, float> CurrentAccumMap;

    const float SourceV = (BatteryVoltage > 0.f) ? BatteryVoltage : DefaultVoltage;

    TArray<AWire*> FirstNext;
    TArray<float>  FirstBlockR;
    CollectNextWiresWithBlockR(FirstNext, FirstBlockR, VoltageMap);

    // V = I * R_total 로 전체 전류 계산 (옴의 법칙)
    // 전선 R=0, 저항은 블럭(Metal/Copper)에서만 나옴
    float TotalR = Resistance;
    if (FirstNext.Num() == 1)
    {
        TSet<AWire*> Rv2;
        Rv2.Add(this);
        TotalR = Resistance + FirstBlockR[0] + FirstNext[0]->CalcSeriesResistance(Rv2);
    }
    else if (FirstNext.Num() > 1)
    {
        float InvR = 0.f;
        for (int32 i = 0; i < FirstNext.Num(); ++i)
        {
            TSet<AWire*> Rv2;
            Rv2.Add(this);
            const float BranchR = FirstBlockR[i] + FirstNext[i]->CalcSeriesResistance(Rv2);
            if (BranchR > 0.001f)
                InvR += 1.f / BranchR;
        }
        TotalR = Resistance + (InvR > 0.f ? 1.f / InvR : 0.f);
    }

    // 실제 저항(블럭)이 없으면 무부하 상태 → 전압만 전달, 전류 = 0
    // 저항이 있을 때만 옴의 법칙으로 전류 계산
    const float MinMeaningfulR = 0.001f;
    const float SourceI = (TotalR > MinMeaningfulR) ? SourceV / TotalR : 0.f;
    PropagateVoltage(SourceV, SourceI, VoltageMap, CurrentAccumMap, IncomingCountMap);
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

    TArray<AWire*> UpstreamWires;
    float TotalUpstreamCurrent = 0.f;
    float TotalUpstreamVoltage = 0.f;
    ATransformation_actor* UpstreamBlock = nullptr;

    if (ConnectionSphere)
    {
        // 컴포넌트 단위로 겹침 체크 → 상대의 END sphere가 내 START sphere에 직접 닿은 경우만
        TArray<UPrimitiveComponent*> OverlappingComps;
        ConnectionSphere->GetOverlappingComponents(OverlappingComps);

        for (UPrimitiveComponent* Comp : OverlappingComps)
        {
            if (!Comp) continue;
            AActor* A = Comp->GetOwner();
            if (!A || A == this) continue;

            if (AWire* OtherWire = Cast<AWire>(A))
            {
                if (SourceWire != nullptr && OtherWire != SourceWire)
                    continue;

                // 내 START sphere에 닿은 컴포넌트가 정확히 상대의 END sphere일 때만 upstream
                if (Comp != OtherWire->GetEndSphere())
                    continue;

                // 전압이 0이어도 켜진 전선이면 전류는 합산 (합류 노드 KCL)
                if (OtherWire->IsPowered())
                {
                    UpstreamWires.AddUnique(OtherWire);
                    TotalUpstreamCurrent += OtherWire->GetEffectiveCurrent();
                    // 전압은 가장 높은 값 사용 (같은 소스면 같아야 하므로 max가 정확)
                    TotalUpstreamVoltage = FMath::Max(TotalUpstreamVoltage, OtherWire->GetEffectiveVoltage());
                }
            }
            else if (A->ActorHasTag(FName("Metal")) || A->ActorHasTag(FName("Copper")))
            {
                if (SourceWire != nullptr) continue;
                ATransformation_actor* Block = Cast<ATransformation_actor>(A);
                if (Block && !UpstreamBlock && Block->IsElectrified() && Block->GetEffectiveVoltage() > 0.f)
                    UpstreamBlock = Block;
            }
        }
    }

    bool bFoundPower = false;
    if (bIsBatterySource)
        bFoundPower = bPoweredBySource;
    else if (UpstreamWires.Num() > 0)
        bFoundPower = true;
    else if (UpstreamBlock && UpstreamBlock->GetEffectiveVoltage() > 0.f)
        bFoundPower = true;

    // 비-배터리 전선: 마지막 PropagateVoltage 이후 일정 시간이 지나면 전원 리셋
    // - 타이머 위상 차이(배터리 solve보다 폴링이 먼저 돌 때)에 유예 시간 부여
    // - 실제로 회로에서 빠진 전선은 solve가 안 오므로 타임아웃 후 자동 꺼짐
    if (!bIsBatterySource && !bFoundPower)
    {
        const float Now     = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
        const float Timeout = RefreshInterval * 2.5f;   // 솔브 2.5 사이클 유예
        if ((Now - LastSolveTimeSeconds) > Timeout)
        {
            bPoweredBySource     = false;
            bCircuitSolved       = false;
            LastSolveTimeSeconds = -999.f;
        }
    }

    SetPoweredByMetal(bFoundPower);

if (bIsBatterySource && bPoweredFinal)
{
    // 끝점 스캔을 TriggerCircuitSolve() 이전에 실행해야
    // CollectNextWiresWithBlockR이 올바르게 동작함
    if (ConnectionSphereEnd)
    {
        TArray<UPrimitiveComponent*> OverlappingComps;
        ConnectionSphereEnd->GetOverlappingComponents(OverlappingComps);
        for (UPrimitiveComponent* Comp : OverlappingComps)
        {
            if (!Comp) continue;
            AActor* A = Comp->GetOwner();
            if (!A || A == this) continue;
            if (AWire* DownWire = Cast<AWire>(A))
            {
                // 내 END sphere에 닿은 컴포넌트가 정확히 상대의 START sphere일 때만 연결
                if (Comp == DownWire->GetStartSphere())
                    ConnectedWires.AddUnique(DownWire);
            }
            else if (A->ActorHasTag(FName("Metal")) || A->ActorHasTag(FName("Copper")))
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

float PrevV = 0.f, PrevI = 0.f;
AWire* DominantUpstream = nullptr;

if (UpstreamWires.Num() > 0)
{
    // ★ 전압·전류는 기존 로직 그대로 (볼트 법칙 유지)
    // 합류 노드: 전압은 max (같은 소스 → 같아야 함), 전류는 KCL 합산
    PrevV = TotalUpstreamVoltage;   // 이미 max로 축적됨
    PrevI = TotalUpstreamCurrent;

    // ★ dominant는 '온도 따라가기' 타겟 고르는 용도로만 사용 (V/I엔 절대 안 씀)
    float BestPower = -1.f;
    for (AWire* W : UpstreamWires)
    {
        const float P = W->GetEffectiveVoltage() * W->GetEffectiveCurrent();
        if (P > BestPower) { BestPower = P; DominantUpstream = W; }
    }
}
else if (UpstreamBlock && UpstreamBlock->GetEffectiveVoltage() > 0.f)
{
    PrevV = UpstreamBlock->GetEffectiveVoltage();
    PrevI = UpstreamBlock->GetEffectiveCurrent();
}

// ★ 끝에 2개 이상 들어오면 합류 노드
bIsMergeNode      = (UpstreamWires.Num() >= 2);
HeatFollowTargetC = (bIsMergeNode && DominantUpstream)
    ? DominantUpstream->GetWireTemperature() : -1.f;

// PropagateVoltage가 이미 정확히 계산해서 bCircuitSolved를 세팅했으면
// 폴링이 덮어쓰지 않음 → 값 안정화
if (!bCircuitSolved)
{
    if (bIsParallel && ParallelBranchCount > 1 && !bIsMergeNode)
    {
        EffectiveVoltage = PrevV;
        EffectiveCurrent = PrevI / float(ParallelBranchCount);
        CachedCircuitText  = FString::Printf(TEXT("[병렬가지 1/%d] V:%.2f I:%.2fA"), ParallelBranchCount, EffectiveVoltage, EffectiveCurrent);
        CachedCircuitColor = FColor::Green;
    }
    else
    {
        EffectiveVoltage = PrevV;
        EffectiveCurrent = PrevI;
        CachedCircuitText  = bIsMergeNode
            ? FString::Printf(TEXT("[합류] V:%.2f I:%.2fA"), EffectiveVoltage, EffectiveCurrent)
            : FString::Printf(TEXT("[직렬] V:%.2f I:%.2fA"), EffectiveVoltage, EffectiveCurrent);
        CachedCircuitColor = bIsMergeNode ? FColor::Orange : FColor::Cyan;
    }
}

    if (ConnectionSphereEnd)
{
    TArray<UPrimitiveComponent*> OverlappingComps;
    ConnectionSphereEnd->GetOverlappingComponents(OverlappingComps);
    for (UPrimitiveComponent* Comp : OverlappingComps)
    {
        if (!Comp) continue;
        AActor* A = Comp->GetOwner();
        if (!A || A == this) continue;
        if (AWire* DownWire = Cast<AWire>(A))
        {
            // 내 END sphere에 닿은 컴포넌트가 정확히 상대의 START sphere일 때만 연결
            if (Comp == DownWire->GetStartSphere())
                ConnectedWires.AddUnique(DownWire);
        }
        else if (A->ActorHasTag(FName("Metal")) || A->ActorHasTag(FName("Copper")))
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
        if (UpstreamWires.Num() > 0 && !SparkComponentStart)
            SparkComponentStart = UNiagaraFunctionLibrary::SpawnSystemAttached(
                SparkEffect, ConnectionSphere, NAME_None,
                FVector::ZeroVector, FRotator::ZeroRotator,
                EAttachLocation::SnapToTarget, true);
        else if (UpstreamWires.Num() == 0 && SparkComponentStart)
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
    TArray<AActor*> Overlapping;
    ConnectionSphereEnd->GetOverlappingActors(Overlapping);

    //UE_LOG(LogTemp, Warning, TEXT("[%s] 끝점 감지 수: %d"), *GetName(), Overlapping.Num());

    for (AActor* A : Overlapping)
    {
        if (!A || A == this) continue;

        /*UE_LOG(LogTemp, Warning, TEXT("  끝점에 닿음: %s (Metal:%d Copper:%d)"),
            A->GetName(),
            A->ActorHasTag(FName("Metal")) ? 1 : 0,
            A->ActorHasTag(FName("Copper")) ? 1 : 0);*/

        if (AWire* DownWire = Cast<AWire>(A))
            ConnectedWires.AddUnique(DownWire);
        else if (A->ActorHasTag(FName("Metal")) || A->ActorHasTag(FName("Copper")))
            if (ATransformation_actor* Block = Cast<ATransformation_actor>(A))
                ConnectedActors.AddUnique(Block);
    }
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