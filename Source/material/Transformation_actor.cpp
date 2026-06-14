#include "Transformation_actor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "materialCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Temperature.h"
#include "Wire.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SplineComponent.h"
#include "Components/MeshComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

// ============================================================================
//  Constructor
// ============================================================================
// ============================================================================
//  Constructor
// ============================================================================
ATransformation_actor::ATransformation_actor()
{
    PrimaryActorTick.bCanEverTick = true;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    SetRootComponent(MeshComp);
    MeshComp->SetMobility(EComponentMobility::Movable);
    MeshComp->SetCollisionProfileName(TEXT("PhysicsActor"));
    MeshComp->SetGenerateOverlapEvents(true);

    // ============================================================
    //  공용 메시 로드 (모든 폼 동일 + 시작 메시도 Box1으로)
    // ============================================================
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BoxMesh(
        TEXT("/Game/modeling/Object/Box/Box1.Box1"));
    if (BoxMesh.Succeeded())
        MeshComp->SetStaticMesh(BoxMesh.Object); // ★ 시작 메시 바로 적용

    // ============================================================
    //  머터리얼 로드
    // ============================================================
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MetalMatFinder(
        TEXT("/DatasmithContent/Materials/FBXImporter/VRED/BrushedMetal.BrushedMetal"));

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> WoodMatFinder(
        TEXT("/Game/modeling/Texture/M_wood.M_wood"));

    // ★ 고무 머터리얼 변경
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> RubberMatFinder(
        TEXT("/AnimationSharing/AnimSharingBlue.AnimSharingBlue"));

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MagnetMatFinder(
        TEXT("/Game/M_Magnet_Master.M_Magnet_Master"));

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> IceMatFinder(
        TEXT("/Game/ICE.ICE"));

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> CopperMatFinder(
        TEXT("/AnimationSharing/AnimSharingBlue.AnimSharingBlue"));

    // ============================================================
    //  피지컬 머터리얼 로드
    // ============================================================
    static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> PM_Ice(
        TEXT("/Game/Block/PM_Ice.PM_Ice"));

    static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> PM_Magnet(
        TEXT("/Game/Block/PM_Magent.PM_Magent"));

    static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> PM_Metal(
        TEXT("/Game/Block/PM_Metal.PM_Metal"));

    static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> PM_Rubber(
        TEXT("/Game/Block/PM_Rubber.PM_Rubber"));

    static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> PM_Wood(
        TEXT("/Game/Block/PM_Wood.PM_Wood"));

    // ============================================================
    //  BurnMaterial (Wood 불탈 때 사용)
    // ============================================================
    if (WoodMatFinder.Succeeded())
        BurnMaterial = WoodMatFinder.Object;

    // ============================================================
    //  Arrow Effect 블루프린트
    // ============================================================
    static ConstructorHelpers::FClassFinder<AActor> ArrowBP(
        TEXT("/Game/modeling/Object/Arrow/Arrow_Effect"));
    if (ArrowBP.Succeeded())
        ArrowEffectClass = ArrowBP.Class;

    // ============================================================
    //  FormSpecs 기본값 세팅
    //  PM이 모든 물리값 담당 → bOverrideMass/Damping 건드리지 않음
    // ============================================================

    // ── Metal ──
    {
        FBlockFormSpec Spec;
        Spec.Form = EBlockForm::Metal;
        Spec.bSimulatePhysics = true;
        if (BoxMesh.Succeeded())
            Spec.Mesh = BoxMesh.Object;
        if (MetalMatFinder.Succeeded())
            Spec.Materials.Add(MetalMatFinder.Object);
        if (PM_Metal.Succeeded())
            Spec.PhysMat = PM_Metal.Object;
        FormSpecs.Add(Spec);
    }

    // ── Copper ──
    {
        FBlockFormSpec Spec;
        Spec.Form = EBlockForm::Copper;
        Spec.bSimulatePhysics = true;
        if (BoxMesh.Succeeded())
            Spec.Mesh = BoxMesh.Object;
        if (CopperMatFinder.Succeeded())
            Spec.Materials.Add(CopperMatFinder.Object);
        if (PM_Metal.Succeeded())
            Spec.PhysMat = PM_Metal.Object; // 구리는 PM_Metal 공유
        FormSpecs.Add(Spec);
    }

    // ── Ice ──
    {
        FBlockFormSpec Spec;
        Spec.Form = EBlockForm::Ice;
        Spec.bSimulatePhysics = true;
        if (BoxMesh.Succeeded())
            Spec.Mesh = BoxMesh.Object;
        if (IceMatFinder.Succeeded())
            Spec.Materials.Add(IceMatFinder.Object);
        if (PM_Ice.Succeeded())
            Spec.PhysMat = PM_Ice.Object;
        FormSpecs.Add(Spec);
    }

    // ── Rubber ──
    {
        FBlockFormSpec Spec;
        Spec.Form = EBlockForm::Rubber;
        Spec.bSimulatePhysics = true;
        if (BoxMesh.Succeeded())
            Spec.Mesh = BoxMesh.Object;
        if (RubberMatFinder.Succeeded())
            Spec.Materials.Add(RubberMatFinder.Object);
        if (PM_Rubber.Succeeded())
            Spec.PhysMat = PM_Rubber.Object;
        FormSpecs.Add(Spec);
    }

    // ── Wood ──
    {
        FBlockFormSpec Spec;
        Spec.Form = EBlockForm::Wood;
        Spec.bSimulatePhysics = true;
        if (BoxMesh.Succeeded())
            Spec.Mesh = BoxMesh.Object;
        if (WoodMatFinder.Succeeded())
            Spec.Materials.Add(WoodMatFinder.Object);
        if (PM_Wood.Succeeded())
            Spec.PhysMat = PM_Wood.Object;
        FormSpecs.Add(Spec);
    }

    // ── Magnet ──
    {
        FBlockFormSpec Spec;
        Spec.Form = EBlockForm::Magnet;
        Spec.bSimulatePhysics = true;
        if (BoxMesh.Succeeded())
            Spec.Mesh = BoxMesh.Object;
        if (MagnetMatFinder.Succeeded())
            Spec.Materials.Add(MagnetMatFinder.Object);
        if (PM_Magnet.Succeeded())
            Spec.PhysMat = PM_Magnet.Object;
        FormSpecs.Add(Spec);
    }

    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SteamFX(
        TEXT("/Game/modeling/Effect/Smoke.Smoke"));
    if (SteamFX.Succeeded())
        SteamEffect = SteamFX.Object;
}

// ============================================================================
//  공용 유틸리티
// ============================================================================
float ATransformation_actor::CalcReceivedPower(float DistCm) const
{
    if (!CurrentFire)
        return 0.f;

    if (CurrentFire->Temperature <= 1.0f)
        return 0.f;

    if (CurrentFire->MaxHeatDistance > 0.f && DistCm > CurrentFire->MaxHeatDistance)
        return 0.f;

    const float DistM = FMath::Max(DistCm / 100.0f, 0.05f);
    const float PtotalW = CurrentFire->GetTotalRadiantPowerW();
    float ReceivedW = (PtotalW / (4.0f * PI * DistM * DistM)) * EffectiveAreaM2;

    if (CurrentFire->MaxHeatDistance > 0.f)
    {
        ReceivedW *= FMath::Clamp(1.0f - (DistCm / CurrentFire->MaxHeatDistance), 0.0f, 1.0f);
    }
    return ReceivedW;
}

void ATransformation_actor::SetStencilSafe(int32 NewValue, bool bDepthOn)
{
    if (!MeshComp)
        return;

    if (CachedStencilValue != NewValue)
    {
        MeshComp->SetCustomDepthStencilValue(NewValue);
        CachedStencilValue = NewValue;
    }
    if (bCachedDepthOn != bDepthOn)
    {
        MeshComp->SetRenderCustomDepth(bDepthOn);
        bCachedDepthOn = bDepthOn;
    }
}

// ★ Metal/Copper에 따라 와이어 감지 반경이 다름
float ATransformation_actor::GetWireSenseRadius() const
{
    if (CurrentForm == EBlockForm::Copper)
        return FMath::Max(MeshComp->Bounds.SphereRadius + CopperWireSenseExtraRadius, 5.f);
    return FMath::Max(MeshComp->Bounds.SphereRadius + WireSenseExtraRadius, 5.f);
}

// ============================================================================
//  BeginPlay
// ============================================================================
void ATransformation_actor::BeginPlay()
{
    Super::BeginPlay();

    SetStencilSafe(0, false);

    if (!CycleOrder.Contains(EBlockForm::Magnet))
        CycleOrder.Add(EBlockForm::Magnet);

    if (!CycleOrder.Contains(EBlockForm::Copper))
        CycleOrder.Add(EBlockForm::Copper);

    if (const FBlockFormSpec *Spec = FindSpec(CurrentForm))
        ApplySpec(*Spec); // ← ApplySpec 내부에서 bFixedInPlace 체크하므로 OK

    if (bAutoUpdateTags)
        UpdateTagsForForm(CurrentForm);

    if (CurrentForm == EBlockForm::Ice)
    {
        BaseScaleBeforeMelt = MeshComp->GetComponentScale();
        EnterIceMode();
    }
    
    else if (CurrentForm == EBlockForm::Wood)
    {
        BaseScaleBeforeBurn = MeshComp->GetComponentScale();
        EnterWoodMode();
    }
    else if (CurrentForm == EBlockForm::Magnet)
    {
        EnterMagnetMode();
    }

    MeshComp->SetNotifyRigidBodyCollision(true);
    MeshComp->OnComponentHit.AddDynamic(this, &ATransformation_actor::OnRubberHit);
    CurrentBounceMultiplier = RubberBounceMultiplier;

    GetWorld()->GetTimerManager().SetTimer(
        RefreshTimerHandle, this,
        &ATransformation_actor::RefreshConnectedWires, 0.2f, true);
}

// ============================================================================
//  OnConstruction
// ============================================================================
void ATransformation_actor::OnConstruction(const FTransform &Transform)
{
    Super::OnConstruction(Transform);

    if (MeshComp && !MeshComp->GetStaticMesh())
    {
        if (const FBlockFormSpec *Spec = FindSpec(CurrentForm))
            ApplySpec(*Spec);

        switch (CurrentForm)
        {
        case EBlockForm::Ice:
            EnterIceMode();
            break;
        case EBlockForm::Wood:
            EnterWoodMode();
            break;
        case EBlockForm::Magnet:
            EnterMagnetMode();
            break;
        default:
            break;
        }
    }
}

// ============================================================================
//  전기 시스템 — Metal + Copper 공용 (IsConductive)
// ============================================================================
void ATransformation_actor::SetPowered(bool bNewPowered)
{
    // ★ Metal 또는 Copper일 때 전기 전도
    if (!IsConductive() || bElectrified == bNewPowered)
        return;
    SetElectrified(bNewPowered);
    EnergizeWiresIfElectrified();
}

// cpp
bool ATransformation_actor::HasSourcePoweredWireRecursive(TSet<const ATransformation_actor *> &Visited) const
{
    if (StoredVoltage > 0.0f)
        return true;

    if (Visited.Contains(this))
        return false;
    Visited.Add(this);

    for (const TObjectPtr<AWire> &W : ConnectedWires)
    {
        if (!W)
            continue;
        if (W->IsSourcePowered())
            return true;

        // 이 전선에 붙은 다른 금속들도 타고 들어감
        if (W->IsPowered())
        {
            for (AActor *Other : W->GetConnectedActors())
            {
                const ATransformation_actor *OtherBlock = Cast<ATransformation_actor>(Other);
                if (!OtherBlock || OtherBlock == this)
                    continue;
                if (OtherBlock->HasSourcePoweredWireRecursive(Visited))
                    return true;
            }
        }
    }
    return false;
}

bool ATransformation_actor::HasSourcePoweredWire() const
{
    TSet<const ATransformation_actor *> Visited;
    return HasSourcePoweredWireRecursive(Visited);
}

void ATransformation_actor::RefreshConnectedWires()
{
    if (!IsConductive() || !MeshComp)
    {
        WiresEnergizedByMetal.Empty();
        ConnectedWires.Empty();
        if (bElectrified)
            SetElectrified(false);
        return;
    }

    if (CurrentForm == EBlockForm::Metal)
        BlockResistance = MetalResistance;
    else if (CurrentForm == EBlockForm::Copper)
        BlockResistance = CopperResistance;

    UWorld *World = GetWorld();
    if (!World)
    {
        ConnectedWires.Empty();
        SetElectrified(false);
        return;
    }

    const FVector Center = MeshComp->Bounds.Origin;
    const float Radius = GetWireSenseRadius();

    FCollisionQueryParams Q(SCENE_QUERY_STAT(MetalWireSense), false);
    Q.AddIgnoredActor(this);

    TArray<FOverlapResult> Hits;
    World->OverlapMultiByObjectType(Hits, Center, FQuat::Identity,
                                    FCollisionObjectQueryParams::AllObjects,
                                    FCollisionShape::MakeSphere(Radius), Q);

    ConnectedWires.Empty();

    // 연결된 전선 목록만 수집 (전원 판단은 StoredVoltage로)
    for (const FOverlapResult &H : Hits)
    {
        if (AWire *Wire = Cast<AWire>(H.GetActor()))
            ConnectedWires.AddUnique(Wire);
    }

    // ★ 전원 판단을 StoredVoltage 기준으로 - 타이머가 전압 안 건드림
    const bool bAnyPowerFound = (StoredVoltage > 0.f);

    const bool bStateChanged = (bElectrified != bAnyPowerFound);
    if (bStateChanged)
        SetElectrified(bAnyPowerFound);

    // 전기가 있으면 연결된 전선들에 전기 전달
    if (bElectrified)
    {
        TSet<TWeakObjectPtr<AWire>> Current;
        for (AWire *Wire : ConnectedWires)
        {
            if (!Wire || Wire->IsSourcePowered())
                continue;
            Wire->SetPoweredByMetal(true);
            Current.Add(Wire);
        }

        WiresEnergizedByMetal = MoveTemp(Current);
    }
    else
    {
        WiresEnergizedByMetal.Empty();
    }
}

void ATransformation_actor::SetElectrified(bool bNewElectrified)
{
    if (bElectrified == bNewElectrified)
        return;
    bElectrified = bNewElectrified;
}

void ATransformation_actor::EnergizeWiresIfElectrified()
{
    if (!bElectrified)
    {
        WiresEnergizedByMetal.Empty();
        return;
    }

    TSet<TWeakObjectPtr<AWire>> Current;
    for (AWire *Wire : ConnectedWires)
    {
        if (!Wire || Wire->IsSourcePowered())
            continue;
        Wire->SetPoweredByMetal(true);
        Current.Add(Wire);
    }
    WiresEnergizedByMetal = MoveTemp(Current);
}
// ============================================================================
//  Tick
// ============================================================================
void ATransformation_actor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ── Ice ──
    if (CurrentForm == EBlockForm::Ice && bHeating && CurrentFire && MeshComp && MeltAlpha < 1.0f)
    {
        const float DistCm = FVector::Dist(CurrentFire->GetActorLocation(), GetActorLocation());
        const float ReceivedPowerW = CalcReceivedPower(DistCm);

        if (ReceivedPowerW > 0.0f)
        {
            SetStencilSafe(CachedStencilValue, true);
            EnergyAccumJ += ReceivedPowerW * DeltaTime * FMath::Max(SimTimeScale, 0.0f);
            MeltAlpha = FMath::Clamp(EnergyAccumJ / FMath::Max(TotalMeltEnergyJ, 1.0f), 0.0f, 1.0f);
            ApplyIceMeltVisual(MeltAlpha);

if (MeltAlpha >= 1.0f && bDestroyWhenMelted)
{
    BeginDelayedDestroy();  // ★ 기존 3줄 대신
    return;
}
        }
    }
    
    // ★ Ice 폼이면 가열 여부와 무관하게 매 프레임 수증기 상태 갱신
// ── Ice ──
    if (CurrentForm == EBlockForm::Ice)
    {
        // ★ 디버그: 매 프레임 상태 출력 (이거 보고 원인 확정)
        float DbgDist = -1.f;
        float DbgRecv = -1.f;
        if (CurrentFire)
        {
            DbgDist = FVector::Dist(CurrentFire->GetActorLocation(), GetActorLocation());
            DbgRecv = CalcReceivedPower(DbgDist);
        }
        GEngine->AddOnScreenDebugMessage(
            (int32)GetUniqueID(), 0.f, FColor::Green,
            FString::Printf(TEXT("[ICE] Alpha=%.4f Accum=%.0f Total=%.0f | bHeating=%d Fire=%d | Dist=%.0f Recv=%.4f | Pending=%d"),
                MeltAlpha, EnergyAccumJ, TotalMeltEnergyJ,
                bHeating ? 1 : 0, CurrentFire ? 1 : 0,
                DbgDist, DbgRecv, bPendingDelayedDestroy ? 1 : 0));

        if (bHeating && CurrentFire && MeshComp && MeltAlpha < 1.0f)
        {
            const float ReceivedPowerW = CalcReceivedPower(DbgDist);
            if (ReceivedPowerW > 0.0f)
            {
                SetStencilSafe(CachedStencilValue, true);
                EnergyAccumJ += ReceivedPowerW * DeltaTime * FMath::Max(SimTimeScale, 0.0f);
                MeltAlpha = FMath::Clamp(EnergyAccumJ / FMath::Max(TotalMeltEnergyJ, 1.0f), 0.0f, 1.0f);
                ApplyIceMeltVisual(MeltAlpha);
            }
        }

        // ★ 가열 여부와 무관하게 파괴 체크 (블럭 밖으로 분리)
if (MeshComp && bDestroyWhenMelted && !bPendingDelayedDestroy)
        {
            const float BaseMax = FMath::Max(BaseScaleBeforeMelt.GetMax(), 0.0001f);
            const float CurRatio = MeshComp->GetComponentScale().GetMax() / BaseMax;
            const float DestroyThreshold = FMath::Clamp(MinScaleRatio, 0.f, 1.f) * 1.1f + 0.01f;  // ★
            if (CurRatio <= DestroyThreshold)
            {
                BeginDelayedDestroy();
                return;
            }
        }

        UpdateSteamEffect();
    }

    // ── Wood ──
    if (CurrentForm == EBlockForm::Wood)
    {
        if (!bIsBurning)
        {
            if (bHeating && CurrentFire && MeshComp)
            {
                const float DistCm = FVector::Dist(CurrentFire->GetActorLocation(), GetActorLocation());
                const float ReceivedPowerW = CalcReceivedPower(DistCm);

                if (ReceivedPowerW > 0.f)
                {
                    const float DeltaT = (ReceivedPowerW * DeltaTime * WoodSimTimeScale) / (CurrentWoodMassKg * SpecificHeatJPerKgK);
                    WoodTemperatureC += DeltaT;

                    const float TempRatio = FMath::Clamp(WoodTemperatureC / WoodIgnitionTempC, 0.f, 1.f);
                    SetStencilSafe(FMath::RoundToInt(TempRatio * 255.f), true);

                    if (WoodTemperatureC >= WoodIgnitionTempC)
                    {
                        bIsBurning = true;
                        CurrentWoodMassKg = WoodMassKg;
                    }
                }
            }
        }
        else
        {
            CurrentWoodMassKg = FMath::Max(CurrentWoodMassKg - BurnRateKgPerSec * DeltaTime, 0.0f);
            BurnAlpha = 1.0f - (CurrentWoodMassKg / FMath::Max(WoodMassKg, 0.01f));

            SetStencilSafe(255, true);
            ApplyWoodBurnVisual(BurnAlpha);

            if (CurrentWoodMassKg <= 0.0f)
            {
                if (bDestroyWhenBurned)
                {
                    Destroy();
                    return;
                }
                bIsBurning = false;
                SetStencilSafe(0, false);
            }
        }
    }

    // ── Metal / Rubber / Magnet / ★Copper 공용 열 ──
    if (CurrentForm == EBlockForm::Metal || CurrentForm == EBlockForm::Rubber || CurrentForm == EBlockForm::Magnet || CurrentForm == EBlockForm::Copper) // ★ Copper도 열 시스템 참여
    {
        UpdateFormHeat(DeltaTime);
    }

    // ── Magnet 물리 ──
    if (CurrentForm == EBlockForm::Magnet && !bDemagnetized)
    {
        UpdateMagnetism(DeltaTime);
    }
    // ★ 블럭 전압/전류 디버깅 표시 (bDebugDraw가 체크되어 있을 때만 보임)
    if (bDebugDraw)
    {
        // 출력할 텍스트 만들기 (예: "V: 220.0 / A: 10.0")
        FString DebugMsg = FString::Printf(TEXT("V: %.1f / A: %.1f"), StoredVoltage, StoredCurrent);

        // 텍스트를 띄울 위치 (블럭 중심에서 위로 60만큼 띄움)
        FVector TextLocation = GetActorLocation() + FVector(0.f, 0.f, 60.f);

        // 화면에 노란색 텍스트 그리기
        DrawDebugString(GetWorld(), TextLocation, DebugMsg, nullptr, FColor::Yellow, 0.f, true, 1.2f);
    }
}

// ============================================================================
//  SetForm
// ============================================================================
void ATransformation_actor::SetForm(EBlockForm NewForm)
{
    UE_LOG(LogTemp, Warning, TEXT("SetForm 호출: %d -> %d"), (int)CurrentForm, (int)NewForm);

    if (CurrentForm == NewForm)
    {
        if (NewForm == EBlockForm::Magnet && bDemagnetized)
        {
            bDemagnetized = false;
            EnterMagnetMode();
            return;
        }
        if (const FBlockFormSpec *Spec = FindSpec(CurrentForm))
            ApplySpec(*Spec);
        return;
    }

    const float SavedMeltAlpha = MeltAlpha;
    const float SavedEnergyAccumJ = EnergyAccumJ;
    ATemperature *SavedFire = CurrentFire;
    const bool bWasHeating = bHeating;
    const FVector SavedCurrentScale = MeshComp ? MeshComp->GetComponentScale() : FVector(1);
    {
        APlayerController *PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
        AmaterialCharacter *PlayerChar = PC ? Cast<AmaterialCharacter>(PC->GetPawn()) : nullptr;
        if (PlayerChar)
        {
            FName Tag;
            switch (NewForm)
            {
            case EBlockForm::Metal:
                Tag = MetalTag;
                break;
            case EBlockForm::Copper:
                Tag = CopperTag;
                break;
            case EBlockForm::Ice:
                Tag = IceTag;
                break;
            case EBlockForm::Rubber:
                Tag = RubberTag;
                break;
            case EBlockForm::Wood:
                Tag = WoodTag;
                break;
            case EBlockForm::Magnet:
                Tag = MagnetTag;
                break;
            default:
                break;
            }
            int32 GaugeVal = PlayerChar->GetGaugeByTag(Tag);
            UE_LOG(LogTemp, Warning, TEXT("SetForm 게이지 검사: %s = %d"), *Tag.ToString(), GaugeVal);

            if (!Tag.IsNone() && GaugeVal <= 0)
            {
                UE_LOG(LogTemp, Warning, TEXT("SetForm: %s 게이지가 0이라 변경 불가"), *Tag.ToString());
                return;
            }
        }
    }

    // 이전 폼 정리
    switch (CurrentForm)
    {
    case EBlockForm::Ice:
        ExitIceMode();
        break;
    case EBlockForm::Wood:
        ExitWoodMode();
        break;
    case EBlockForm::Magnet:
        ExitMagnetMode();
        break;
    case EBlockForm::Metal:
    case EBlockForm::Copper:
        SetElectrified(false);
        WiresEnergizedByMetal.Empty();
        ConnectedWires.Empty();
        break;
    default:
        break;
    }

    FormTemperatureC = 20.f;
    SetStencilSafe(0, false);
    CurrentForm = NewForm;

    if (bAutoUpdateTags)
        UpdateTagsForForm(NewForm);

    if (const FBlockFormSpec *Spec = FindSpec(CurrentForm))
        ApplySpec(*Spec);

    SetStencilSafe(0, false);

    if (MeshComp && SavedMeltAlpha > 0.0f)
        MeshComp->SetWorldScale3D(SavedCurrentScale);

    switch (CurrentForm)
    {
    case EBlockForm::Ice:
        BaseScaleBeforeMelt = SavedCurrentScale;
        EnterIceMode();
        MeltAlpha = SavedMeltAlpha;
        EnergyAccumJ = SavedEnergyAccumJ;
        CurrentFire = SavedFire;
        bHeating = bWasHeating && (CurrentFire != nullptr);
        ApplyIceMeltVisual(MeltAlpha);
        break;
    case EBlockForm::Wood:
        BaseScaleBeforeBurn = SavedCurrentScale;
        EnterWoodMode();
        CurrentFire = SavedFire;
        bHeating = bWasHeating && (CurrentFire != nullptr);
        break;
    case EBlockForm::Magnet:
        EnterMagnetMode();
        break;
    // ★ Copper/Metal: 특별한 Enter 불필요, 타이머 RefreshConnectedWires가 처리
    default:
        break;
    }
}

// ============================================================================
//  게이지 / 폼 사이클
// ============================================================================
void ATransformation_actor::DecreaseGaugeForCurrentTag()
{
    APlayerController *PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC)
        return;
    AmaterialCharacter *PlayerChar = Cast<AmaterialCharacter>(PC->GetPawn());
    if (!PlayerChar)
        return;

    // ★ Copper 추가
    static const FName TagNames[] = {
        TEXT("Rubber"), TEXT("Metal"), TEXT("Copper"),
        TEXT("Ice"), TEXT("Wood"), TEXT("Magnet")};
    for (const FName &Tag : TagNames)
    {
        if (ActorHasTag(Tag))
        {
            PlayerChar->DecreaseGaugeForMaterial(Tag);
            return;
        }
    }
}

void ATransformation_actor::NextForm()
{
    if (CycleOrder.Num() <= 0)
        return;

    APlayerController *PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    AmaterialCharacter *PlayerChar = PC ? Cast<AmaterialCharacter>(PC->GetPawn()) : nullptr;

    int32 StartIdx = CycleOrder.Find(CurrentForm);
    if (StartIdx == INDEX_NONE)
        StartIdx = -1;

    int32 NextIdx = INDEX_NONE;
    for (int32 i = 1; i <= CycleOrder.Num(); ++i)
    {
        int32 TryIdx = (StartIdx + i) % CycleOrder.Num();
        EBlockForm TryForm = CycleOrder[TryIdx];

        if (TryForm == CurrentForm)
            continue;

        if (PlayerChar)
        {
            FName Tag;
            switch (TryForm)
            {
            case EBlockForm::Metal:
                Tag = MetalTag;
                break;
            case EBlockForm::Copper:
                Tag = CopperTag;
                break;
            case EBlockForm::Ice:
                Tag = IceTag;
                break;
            case EBlockForm::Rubber:
                Tag = RubberTag;
                break;
            case EBlockForm::Wood:
                Tag = WoodTag;
                break;
            case EBlockForm::Magnet:
                Tag = MagnetTag;
                break;
            default:
                continue;
            }

            if (PlayerChar->GetGaugeByTag(Tag) <= 0)
                continue;
        }

        NextIdx = TryIdx;
        break;
    }

    if (NextIdx == INDEX_NONE)
    {
        UE_LOG(LogTemp, Warning, TEXT("NextForm: 사용 가능한 폼이 없음 (모든 게이지 0)"));
        return;
    }

    SetForm(CycleOrder[NextIdx]);
    DecreaseGaugeForCurrentTag();
}

// ============================================================================
//  태그
// ============================================================================
void ATransformation_actor::UpdateTagsForForm(EBlockForm Form)
{
    ClearAllFormTags();
    switch (Form)
    {
    case EBlockForm::Ice:
        Tags.AddUnique(IceTag);
        break;
    case EBlockForm::Metal:
        Tags.AddUnique(MetalTag);
        break;
    case EBlockForm::Copper:
        Tags.AddUnique(CopperTag);
        break; // ★
    case EBlockForm::Wood:
        Tags.AddUnique(WoodTag);
        break;
    case EBlockForm::Rubber:
        Tags.AddUnique(RubberTag);
        break;
    case EBlockForm::Magnet:
        Tags.AddUnique(MagnetTag);
        break;
    default:
        break;
    }
}

void ATransformation_actor::ClearAllFormTags()
{
    Tags.Remove(IceTag);
    Tags.Remove(MetalTag);
    Tags.Remove(CopperTag); // ★
    Tags.Remove(WoodTag);
    Tags.Remove(RubberTag);
    Tags.Remove(MagnetTag);
}

// ============================================================================
//  Heating
// ============================================================================
void ATransformation_actor::StartHeating(ATemperature *FireRef)
{
    CurrentFire = FireRef;
    bHeating = (CurrentFire != nullptr);
}

void ATransformation_actor::ReceivePower(float InVoltage, float InCurrent)
{
    if (!IsConductive())
        return;

    const float R = (CurrentForm == EBlockForm::Copper) ? CopperResistance : MetalResistance;
    const float DroppedV = FMath::Max(InVoltage - InCurrent * R, 0.f);

    StoredVoltage = FMath::Max(StoredVoltage, DroppedV);
    StoredCurrent = FMath::Max(StoredCurrent, InCurrent);

    if (StoredVoltage > 0.f)
        SetElectrified(true);
}
void ATransformation_actor::ReceiveHeatEnergy(float EnergyJ, float SourceTempC)
{
    if (CurrentForm != EBlockForm::Ice || !MeshComp || EnergyJ <= 0.f)
        return;

    SetStencilSafe(CachedStencilValue, true);
    EnergyAccumJ += EnergyJ * FMath::Max(SimTimeScale, 0.0f);
    MeltAlpha = FMath::Clamp(EnergyAccumJ / FMath::Max(TotalMeltEnergyJ, 1.0f), 0.0f, 1.0f);
    ApplyIceMeltVisual(MeltAlpha);

    if (MeltAlpha >= 1.0f && bDestroyWhenMelted)
        BeginDelayedDestroy(); 
}

void ATransformation_actor::StopHeating()
{
    bHeating = false;
    CurrentFire = nullptr;

    if (MeshComp)
    {
        if (CurrentForm == EBlockForm::Wood && bIsBurning)
            return;
        if (CurrentForm == EBlockForm::Ice || CurrentForm == EBlockForm::Wood)
            SetStencilSafe(0, false);
        // Metal, Rubber, Magnet, ★Copper: UpdateFormHeat가 냉각 처리
    }
}

// ============================================================================
//  Spec
// ============================================================================
const FBlockFormSpec *ATransformation_actor::FindSpec(EBlockForm Form) const
{
    for (const FBlockFormSpec &S : FormSpecs)
        if (S.Form == Form)
            return &S;
    return nullptr;
}

void ATransformation_actor::ApplySpec(const FBlockFormSpec &Spec)
{
    if (!MeshComp)
        return;

    if (Spec.Mesh && MeshComp->GetStaticMesh() != Spec.Mesh)
        MeshComp->SetStaticMesh(Spec.Mesh);

    for (int32 i = 0; i < Spec.Materials.Num(); ++i)
    {
        if (Spec.Materials[i] && MeshComp->GetMaterial(i) != Spec.Materials[i])
            MeshComp->SetMaterial(i, Spec.Materials[i]);
    }

    if (bFixedInPlace)
    {
        ApplyFixedPhysics();
        return;
    }

    MeshComp->SetSimulatePhysics(Spec.bSimulatePhysics);
    MeshComp->SetLinearDamping(Spec.LinearDamping);
    MeshComp->SetAngularDamping(Spec.AngularDamping);
    if (Spec.PhysMat)
        MeshComp->SetPhysMaterialOverride(Spec.PhysMat);
    if (Spec.bOverrideMass)
        MeshComp->SetMassOverrideInKg(NAME_None, Spec.MassKg, true);
}

// ============================================================================
//  Ice
// ============================================================================
void ATransformation_actor::EnterIceMode()
{
    if (!MeshComp)
        return;
    if (MeltAlpha == 0.0f)
        EnergyAccumJ = 0.0f;
    RecalcIceMassAndEnergy();

    if (!IceMID)
    {
        UMaterialInterface *SrcMat = IceMeltMaterial ? IceMeltMaterial : MeshComp->GetMaterial(0);
        if (SrcMat)
        {
            IceMID = UMaterialInstanceDynamic::Create(SrcMat, this);
        }
    }

    if (IceMID)
        MeshComp->SetMaterial(0, IceMID);
}

void ATransformation_actor::ExitIceMode()
{
    IceMID = nullptr;
    DestroySteamEffect(); // 얼음 폼 종료 시 수증기 제거
}

void ATransformation_actor::RecalcIceMassAndEnergy()
{
    if (!MeshComp || !MeshComp->GetStaticMesh())
    {
        VolumeM3 = 1.0f;
        EffectiveAreaM2 = 1.0f;
        TotalMeltEnergyJ = IceDensityKgM3 * VolumeM3 * LatentHeatJPerKg;
        return;
    }
    const FBoxSphereBounds LB = MeshComp->GetStaticMesh()->GetBounds();
    const FVector SafeScale(FMath::Max(BaseScaleBeforeMelt.X, 0.01f),
                            FMath::Max(BaseScaleBeforeMelt.Y, 0.01f),
                            FMath::Max(BaseScaleBeforeMelt.Z, 0.01f));
    const FVector SizeM = LB.BoxExtent * 2.0f * SafeScale / 100.0f;
    VolumeM3 = FMath::Max(SizeM.X * SizeM.Y * SizeM.Z, 1e-6f);
    EffectiveAreaM2 = FMath::Max3(SizeM.X * SizeM.Y, SizeM.X * SizeM.Z, SizeM.Y * SizeM.Z);
    TotalMeltEnergyJ = FMath::Max(IceDensityKgM3 * VolumeM3 * LatentHeatJPerKg, 1.0f);
}

void ATransformation_actor::ApplyIceMeltVisual(float Alpha01)
{
    if (!MeshComp)
        return;
    const float A = FMath::Clamp(Alpha01, 0.0f, 1.0f);
    const FVector NewScale = FMath::Lerp(BaseScaleBeforeMelt,
        BaseScaleBeforeMelt * FMath::Clamp(MinScaleRatio, 0.f, 1.f), A);
    MeshComp->SetWorldScale3D(NewScale);
    if (IceMID)
        IceMID->SetScalarParameterValue(MeltParamName, A);

        if (MeltAlpha >= 0.99f && bDestroyWhenMelted && !bPendingDelayedDestroy)
        {
            BeginDelayedDestroy();
            return;
        }
}
// ============================================================================
//  Wood
// ============================================================================
void ATransformation_actor::EnterWoodMode()
{
    if (!MeshComp)
        return;
    RecalcWoodMassAndVolume();
    WoodTemperatureC = 20.0f;
    CurrentWoodMassKg = WoodMassKg;
    BurnAlpha = 0.0f;
    bIsBurning = false;

    if (!BurnMID)
    {
        UMaterialInterface *SrcMat = BurnMaterial ? BurnMaterial : MeshComp->GetMaterial(0);
        if (SrcMat)
        {
            BurnMID = UMaterialInstanceDynamic::Create(SrcMat, this);
        }
    }

    if (BurnMID)
        MeshComp->SetMaterial(0, BurnMID);

    ApplyWoodBurnVisual(0.0f);
}

void ATransformation_actor::ExitWoodMode()
{
    bIsBurning = false;
    WoodTemperatureC = 20.0f;
    CurrentWoodMassKg = WoodMassKg;
    BurnAlpha = 0.0f;
    BurnMID = nullptr;
}

void ATransformation_actor::RecalcWoodMassAndVolume()
{
    if (!MeshComp || !MeshComp->GetStaticMesh())
    {
        WoodVolumeM3 = 1.0f;
        WoodMassKg = WoodDensityKgM3 * WoodVolumeM3;
        return;
    }
    const FBoxSphereBounds LB = MeshComp->GetStaticMesh()->GetBounds();
    const FVector SafeScale(FMath::Max(BaseScaleBeforeBurn.X, 0.01f),
                            FMath::Max(BaseScaleBeforeBurn.Y, 0.01f),
                            FMath::Max(BaseScaleBeforeBurn.Z, 0.01f));
    const FVector SizeM = LB.BoxExtent * 2.0f * SafeScale / 100.0f;
    WoodVolumeM3 = FMath::Max(SizeM.X * SizeM.Y * SizeM.Z, 1e-6f);
    WoodMassKg = WoodDensityKgM3 * WoodVolumeM3;
    EffectiveAreaM2 = FMath::Max3(SizeM.X * SizeM.Y, SizeM.X * SizeM.Z, SizeM.Y * SizeM.Z);
}

void ATransformation_actor::ApplyWoodBurnVisual(float Alpha01)
{
    if (!MeshComp)
        return;
    const float A = FMath::Clamp(Alpha01, 0.0f, 1.0f);
    if (BurnMID)
        BurnMID->SetScalarParameterValue(BurnParamName, A);
    const float ScaleRatio = FMath::Pow(FMath::Max(1.0f - A, MinBurnScaleRatio), 1.0f / 3.0f);
    MeshComp->SetWorldScale3D(BaseScaleBeforeBurn * ScaleRatio);
}

// ============================================================================
//  Polarity helpers
// ============================================================================
FVector ATransformation_actor::GetNorthPoleWorldDir() const
{
    if (!MeshComp)
        return FVector::ForwardVector;
    return MeshComp->GetComponentTransform().TransformVectorNoScale(NorthPoleLocalDir).GetSafeNormal();
}

FVector ATransformation_actor::GetSouthPoleWorldDir() const { return -GetNorthPoleWorldDir(); }

// ============================================================================
//  UpdateFormHeat — ★ Copper도 참여 (Metal과 동일 로직)
// ============================================================================
void ATransformation_actor::UpdateFormHeat(float DeltaTime)
{
    if (!MeshComp)
        return;

    float MaxTempC;
    int32 MaxStencil;
    switch (CurrentForm)
    {
    case EBlockForm::Metal:
        MaxTempC = MetalMaxHeatTempC;
        MaxStencil = MetalMaxStencilValue;
        break;
    case EBlockForm::Rubber:
        MaxTempC = RubberMaxHeatTempC;
        MaxStencil = RubberMaxStencilValue;
        break;
    case EBlockForm::Magnet:
        MaxTempC = MagnetCurieTempC;
        MaxStencil = MagnetMaxStencilValue;
        break;
    // ★ Copper: Metal과 같은 열 파라미터 사용 (나중에 분리 가능)
    case EBlockForm::Copper:
        MaxTempC = MetalMaxHeatTempC;
        MaxStencil = MetalMaxStencilValue;
        break;
    default:
        return;
    }

    // 가열 / 냉각
    if (bHeating && CurrentFire)
    {
        const float DistCm = FVector::Dist(CurrentFire->GetActorLocation(), GetActorLocation());
        const float ReceivedW = CalcReceivedPower(DistCm);
        if (ReceivedW > 0.f)
        {
            // ★ Copper는 열전도율이 높으므로 더 빠르게 가열
            const float ConductivityMul = (CurrentForm == EBlockForm::Copper) ? CopperConductivityMultiplier : 1.0f;
            FormTemperatureC += (ReceivedW * DeltaTime * FormHeatSimTimeScale * ConductivityMul) / (FormMassKg * FormSpecificHeatJPerKgK);
        }
    }
    else if (FormTemperatureC > 20.f)
    {
        FormTemperatureC = FMath::Max(FormTemperatureC - FormCoolingRatePerSec * DeltaTime, 20.f);
    }

    FormTemperatureC = FMath::Min(FormTemperatureC, MaxTempC);

    const bool bAtRoomTemp = (FormTemperatureC <= 20.f + KINDA_SMALL_NUMBER);

    const float HeatRatio = bAtRoomTemp ? 0.f
                                        : FMath::Clamp((FormTemperatureC - 20.f) / FMath::Max(MaxTempC - 20.f, 1.f), 0.f, 1.f);

    const int32 StencilValue = FMath::RoundToInt(HeatRatio * MaxStencil);

    if (CurrentForm == EBlockForm::Magnet)
        SetStencilSafe(StencilValue, StencilValue > 0); // 열 없으면 pass OFF
    else
        SetStencilSafe(StencilValue, StencilValue > 0);

    // ── 자석 전용: 점진적 약화 / 소자 / 복구 ──
    if (CurrentForm != EBlockForm::Magnet)
        return;

    if (!bDemagnetized)
    {
        const float PowerRatio = 1.0f - HeatRatio;
        MagnetStrength = BaseMagnetStrength * PowerRatio;
        UpdateMagnetArrowPower(PowerRatio);

        if (HeatRatio >= 1.0f)
        {
            bDemagnetized = true;
            bMagnetCollided = false;
            bMagnetSnapped = false;
            bElectroActive = false;
            MagnetStrength = 0.f;
            BaseMagnetStrength = 0.f;
            TimeSinceLastMagnetRefresh = 0.f;
            OverlappingMetals.Empty();
            OverlappingCoppers.Empty(); // ★
            MagnetContactedWires.Empty();
            PreviousOverlappingMetals.Empty();

            if (SpawnedArrowEffect)
            {
                SpawnedArrowEffect->SetActorHiddenInGame(true);
                UpdateMagnetArrowPower(0.f);
            }
            if (MeshComp->IsSimulatingPhysics())
            {
                MeshComp->SetPhysicsLinearVelocity(FVector::ZeroVector);
                MeshComp->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
            }
        }
    }
    else
    {
        if (HeatRatio < MagnetRecoveryRatio)
        {
            bDemagnetized = false;
            bMagnetCollided = false;
            bMagnetSnapped = false;
            TimeSinceLastMagnetRefresh = 0.f;

            BaseMagnetStrength = bAutoComputeStrength
                                     ? MaxLiftMass * GravityAccel * FMath::Pow(ReferenceDistance, MagneticDecayExponent)
                                     : MagnetStrength;

            const float PowerRatio = 1.0f - HeatRatio;
            MagnetStrength = BaseMagnetStrength * PowerRatio;
            RefreshOverlappingMetals();

            if (SpawnedArrowEffect)
                UpdateMagnetArrowPower(PowerRatio);
        }
        else if (SpawnedArrowEffect && !SpawnedArrowEffect->IsHidden())
        {
            SpawnedArrowEffect->SetActorHiddenInGame(true);
        }
    }
}

// ============================================================================
//  UpdateMagnetArrowPower
// ============================================================================
void ATransformation_actor::UpdateMagnetArrowPower(float PowerRatio)
{
    if (!SpawnedArrowEffect)
        return;
    const float Power = BaseArrowPower * FMath::Max(PowerRatio, 0.f);
    if (FProperty *P = SpawnedArrowEffect->GetClass()->FindPropertyByName(TEXT("Power")))
    {
        if (FDoubleProperty *D = CastField<FDoubleProperty>(P))
            D->SetPropertyValue_InContainer(SpawnedArrowEffect, (double)Power);
        else if (FFloatProperty *F = CastField<FFloatProperty>(P))
            F->SetPropertyValue_InContainer(SpawnedArrowEffect, Power);
    }
}

// ============================================================================
//  UpdateMagnetElectroBoost
// ============================================================================
void ATransformation_actor::UpdateMagnetElectroBoost()
{
    MagnetContactedWires.Empty();
    bool bAnyPowered = false;
    float TotalCurrent = 0.f;

    const FVector MyLoc = GetActorLocation();

    FCollisionQueryParams Q(SCENE_QUERY_STAT(ElectroBoost), false);
    Q.AddIgnoredActor(this);

    TArray<FOverlapResult> Hits;
    GetWorld()->OverlapMultiByObjectType(Hits, MyLoc, FQuat::Identity,
                                         FCollisionObjectQueryParams::AllObjects,
                                         FCollisionShape::MakeSphere(WireContactRadius), Q);

    for (const FOverlapResult &H : Hits)
    {
        AWire *Wire = Cast<AWire>(H.GetActor());
        if (!Wire || !Wire->IsPowered())
            continue;

        if (MagnetContactedWires.Contains(Wire))
            continue;

        bool bClose = false;

        if (USplineComponent *Spline = Wire->GetSplineComponent())
        {
            const FVector Closest = Spline->FindLocationClosestToWorldLocation(MyLoc, ESplineCoordinateSpace::World);
            bClose = FVector::Dist(MyLoc, Closest) <= WireContactRadius;

            if (!bClose)
            {
                const int32 NumPts = Spline->GetNumberOfSplinePoints();
                for (int32 i = 0; i < NumPts; ++i)
                {
                    if (FVector::Dist(MyLoc, Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World)) <= WireContactRadius)
                    {
                        bClose = true;
                        break;
                    }
                }
            }
        }
        else
        {
            bClose = FVector::Dist(MyLoc, Wire->GetActorLocation()) <= WireContactRadius;
        }

        if (bClose)
        {
            bAnyPowered = true;
            TotalCurrent += Wire->GetWireTemperature() / 100.f;
            MagnetContactedWires.AddUnique(Wire);
        }
    }

    bElectroActive = bAnyPowered;
    MagnetStrength = bElectroActive
                         ? BaseMagnetStrength * FMath::Clamp(TotalCurrent, 1.0f, ElectroBoostMultiplier)
                         : BaseMagnetStrength;
}

// ============================================================================
//  ApplyInducedMagnetism
// ============================================================================
void ATransformation_actor::ApplyInducedMagnetism()
{
    const FVector MagnetLoc = GetActorLocation();
    const TArray<UPrimitiveComponent *> MetalArray = OverlappingMetals.Array();
    const int32 Num = MetalArray.Num();
    if (Num < 2)
        return;

    for (int32 i = 0; i < Num; ++i)
    {
        UPrimitiveComponent *MetalA = MetalArray[i];
        if (!IsValid(MetalA) || !MetalA->IsSimulatingPhysics())
            continue;

        const FVector MetalALoc = MetalA->GetComponentLocation();
        const float DistAToMagnet = FVector::Dist(MetalALoc, MagnetLoc);
        if (DistAToMagnet > MinDistanceForInduction)
            continue;

        const float InducedStr = CalculateInducedStrength(DistAToMagnet, MagnetStrength);
        const FVector MagnetToA = (MetalALoc - MagnetLoc).GetSafeNormal();

        for (int32 j = i + 1; j < Num; ++j)
        {
            UPrimitiveComponent *MetalB = MetalArray[j];
            if (!IsValid(MetalB) || !MetalB->IsSimulatingPhysics())
                continue;

            const FVector AtoB = MetalB->GetComponentLocation() - MetalALoc;
            const float DistAtoB = AtoB.Size();
            if (DistAtoB < 10.f || DistAtoB > InductionRange)
                continue;

            const FVector Dir = AtoB / DistAtoB;
            const float Alignment = FVector::DotProduct(Dir, MagnetToA);

            float ForceMag = (InducedStr * InductionStrengthRatio * FMath::Abs(Alignment)) / FMath::Pow(DistAtoB, MagneticDecayExponent);
            ForceMag *= FMath::Clamp(MetalB->GetMass() / 10.0f, 0.5f, 2.0f);

            const FVector VelB = MetalB->GetPhysicsLinearVelocity();
            float VelDamp = 1.0f;
            const float VelToA = FVector::DotProduct(VelB, Dir);
            if (VelToA > MaxAttractVelocity * 0.5f)
                VelDamp = FMath::Clamp(1.0f - (VelToA / MaxAttractVelocity), 0.3f, 1.0f);

            FVector Force = (Dir * ForceMag * VelDamp * Alignment) + (-VelB * VelocityDampingFactor * 0.5f * MetalB->GetMass());
            Force = Force.GetClampedToMaxSize(MaxInducedForceClamp);

            MetalB->AddForce(Force, NAME_None, false);
            MetalA->AddForce(-Force * 0.5f, NAME_None, false);
        }
    }
}

float ATransformation_actor::CalculateInducedStrength(float Dist, float BaseStr) const
{
    const float Safe = FMath::Max(Dist, 1.0f);
    return BaseStr * FMath::Clamp(1.0f / FMath::Pow(Safe / MinDistanceForInduction, 1.5f), 0.0f, 1.0f);
}

// ============================================================================
//  EnterMagnetMode
// ============================================================================
void ATransformation_actor::EnterMagnetMode()
{
    if (!MeshComp)
        return;

    bMagnetCollided = false;
    bMagnetSnapped = false;

    if (bAutoComputeStrength)
        MagnetStrength = MaxLiftMass * GravityAccel * FMath::Pow(ReferenceDistance, MagneticDecayExponent);

    BaseMagnetStrength = MagnetStrength;
    bDemagnetized = false;
    bElectroActive = false;
    TimeSinceLastMagnetRefresh = 0.f;
    OverlappingMetals.Empty();
    OverlappingCoppers.Empty(); // ★
    MagnetContactedWires.Empty();
    PreviousOverlappingMetals.Empty();

    if (bFixedInPlace)
    {
        ApplyFixedPhysics();
    }
    else
    {
        MeshComp->SetSimulatePhysics(true);
        MeshComp->SetPhysicsLinearVelocity(FVector::ZeroVector);
        MeshComp->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }

    FormTemperatureC = 20.f;
    BaseArrowPower = ArrowPower;
    SetStencilSafe(0, true);

    RefreshOverlappingMetals();

    if (bShowFieldArrows && ArrowEffectClass)
    {
        // ★ 1초 타이머 제거! (즉시 스폰되도록 수정)
        const FQuat OffQ = FRotator(90.f, 0.f, 0.f).Quaternion();
        FTransform T((GetActorQuat() * OffQ).Rotator(), GetActorLocation());

        AActor *Arrow = GetWorld()->SpawnActorDeferred<AActor>(
            ArrowEffectClass, T, this, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

        if (!Arrow)
            return;

        // 파라미터 세팅
        auto SetFloat = [Arrow](const TCHAR *Name, float Val)
        {
            if (FProperty *P = Arrow->GetClass()->FindPropertyByName(Name))
            {
                if (FDoubleProperty *D = CastField<FDoubleProperty>(P))
                    D->SetPropertyValue_InContainer(Arrow, (double)Val);
                else if (FFloatProperty *F = CastField<FFloatProperty>(P))
                    F->SetPropertyValue_InContainer(Arrow, Val);
            }
        };
        SetFloat(TEXT("Power"), ArrowPower);
        SetFloat(TEXT("X"), ArrowX);
        SetFloat(TEXT("Y"), ArrowY);

        UGameplayStatics::FinishSpawningActor(Arrow, T);

        TArray<USceneComponent *> AllComps;
        Arrow->GetRootComponent()->GetChildrenComponents(true, AllComps);
        AllComps.Add(Arrow->GetRootComponent());

        for (USceneComponent *C : AllComps)
        {
            C->SetMobility(EComponentMobility::Movable);

            // ★ 화살표에도 자석과 똑같이 Custom Depth Stencil 적용 (매우 중요)
            if (UPrimitiveComponent *PrimC = Cast<UPrimitiveComponent>(C))
            {
                PrimC->SetRenderCustomDepth(true);
                PrimC->SetCustomDepthStencilValue(255); // 현재 포스트 프로세스에서 감지하는 스텐실 값(예: 255)으로 설정
                PrimC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
        }

        SpawnedArrowEffect = Arrow;

        if (SpawnedArrowEffect && MeshComp)
        {
            SpawnedArrowEffect->AttachToComponent(MeshComp, FAttachmentTransformRules::KeepRelativeTransform);

            FVector MinBounds, MaxBounds;
            MeshComp->GetLocalBounds(MinBounds, MaxBounds);

            float TopZ = MaxBounds.Z - 50.f;
            SpawnedArrowEffect->SetActorRelativeLocation(FVector(0.f, 0.f, TopZ));
            SpawnedArrowEffect->SetActorRelativeRotation(FRotator(90.f, 0.f, 0.f));

            // X축(긴 방향) 기준으로 화살표 스케일 계산
            // MaxBounds는 half-extent이므로 실제 길이 = MaxBounds * 2
            float ScaleX = (MaxBounds.X / 50.f) * 0.5f;
            float ScaleY = (MaxBounds.Y / 50.f) * 0.5f;
            float ScaleZ = (MaxBounds.Z / 50.f) * 0.3f;
            SpawnedArrowEffect->SetActorRelativeScale3D(FVector(ScaleX, ScaleY, ScaleZ));
        }

        RefreshArrowVisibility();
    }
}

void ATransformation_actor::ExitMagnetMode()
{
    bMagnetCollided = false;
    bMagnetSnapped = false;
    bDemagnetized = false;
    bElectroActive = false;
    MagnetStrength = 0.f;
    BaseMagnetStrength = 0.f;
    TimeSinceLastMagnetRefresh = 0.f;
    FormTemperatureC = 20.f;
    OverlappingMetals.Empty();
    OverlappingCoppers.Empty(); // ★
    MagnetContactedWires.Empty();
    PreviousOverlappingMetals.Empty();

    if (SpawnedArrowEffect)
    {
        SpawnedArrowEffect->Destroy();
        SpawnedArrowEffect = nullptr;
    }
    SetStencilSafe(0, false);
}

void ATransformation_actor::OnMagnetHit(UPrimitiveComponent *, AActor *, UPrimitiveComponent *, FVector, const FHitResult &) {}

// ============================================================================
//  ★ UpdateMagnetism — Copper 반자성 척력 추가
// ============================================================================
void ATransformation_actor::UpdateMagnetism(float DeltaTime)
{
    if (bDemagnetized || !MeshComp)
        return;
    if (MeshComp->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
        return;

    if (bMagnetSnapped)
    {
        if (MeshComp->IsSimulatingPhysics())
            MeshComp->SetSimulatePhysics(false);
        return;
    }

    TimeSinceLastMagnetRefresh += DeltaTime;
    if (TimeSinceLastMagnetRefresh >= MagnetRefreshInterval)
    {
        TimeSinceLastMagnetRefresh = 0.f;
        UpdateMagnetElectroBoost();
        RefreshOverlappingMetals();
    }

    // ────────────────────────────────────────────────
    //  ★ Copper 반자성 척력 (자석이 구리를 약하게 밀어냄)
    //  실제 반자성: 자기장 변화에 반대 방향으로 약한 힘 발생
    // ────────────────────────────────────────────────
    if (OverlappingCoppers.Num() > 0)
    {
        const FVector MagnetLoc = GetActorLocation();

        for (auto It = OverlappingCoppers.CreateIterator(); It; ++It)
        {
            UPrimitiveComponent *CopperComp = It->Get();
            if (!IsValid(CopperComp))
            {
                It.RemoveCurrent();
                continue;
            }
            if (!CopperComp->IsSimulatingPhysics())
                continue;

            AActor *CopperOwner = CopperComp->GetOwner();
            if (!CopperOwner)
            {
                It.RemoveCurrent();
                continue;
            }

            const FVector CopperLoc = CopperComp->GetComponentLocation();
            const FVector ToCopper = CopperLoc - MagnetLoc;
            const float Dist = ToCopper.Size();

            if (Dist < 1.f || Dist > DiamagneticMaxRange)
                continue;

            // 반자성 척력: 거리 감쇠 + 밀어내는 방향
            const FVector RepelDir = ToCopper / Dist;
            const float SafeDist = FMath::Max(Dist, MinDistance);

            float RepelMag = DiamagneticRepulsionForce / FMath::Pow(SafeDist, DiamagneticDecayExponent);
            RepelMag *= FMath::Clamp(CopperComp->GetMass() / 5.0f, 0.5f, 2.0f);

            // 속도 댐핑 — 이미 빠르게 밀려나고 있으면 힘 줄임
            const FVector CurVel = CopperComp->GetPhysicsLinearVelocity();
            const float VelAway = FVector::DotProduct(CurVel, RepelDir);
            if (VelAway > MaxAttractVelocity * 0.5f)
                RepelMag *= FMath::Clamp(1.0f - (VelAway / MaxAttractVelocity), 0.2f, 1.0f);

            const FVector RepelForce = (RepelDir * RepelMag).GetClampedToMaxSize(MaxForceClamp * 0.1f);

            CopperComp->AddForce(RepelForce, NAME_None, false);

            // 반작용: 자석도 약간 밀림
            if (MeshComp->IsSimulatingPhysics())
                MeshComp->AddForce(-RepelForce * 0.1f, NAME_None, false);

            if (bDebugDraw)
            {
                DrawDebugLine(GetWorld(), MagnetLoc, CopperLoc,
                              FColor::Orange, false, 0.f, 0, 2.f);
            }
        }
    }

    if (OverlappingMetals.Num() == 0)
        return;

    {
        // InitialImpulse
        if (bApplyInitialImpulse)
        {
            for (UPrimitiveComponent *Comp : OverlappingMetals)
            {
                if (!IsValid(Comp) || !Comp->IsSimulatingPhysics())
                    continue;
                AActor *OwnerActor = Comp->GetOwner();
                if (!OwnerActor || !OwnerActor->ActorHasTag(MetalTag))
                    continue;
                if (!PreviousOverlappingMetals.Contains(Comp))
                {
                    const FVector Dir = (GetActorLocation() - Comp->GetComponentLocation()).GetSafeNormal();
                    Comp->AddImpulse(Dir * InitialImpulseStrength * Comp->GetMass());
                }
            }
        }
        PreviousOverlappingMetals = OverlappingMetals;

        const FVector MagnetLoc = GetActorLocation();
        const FVector MyNorth = GetNorthPoleWorldDir();
        const FVector MagnetFwd = MeshComp->GetForwardVector();
        const bool bMagSim = MeshComp->IsSimulatingPhysics();
        const float StrMul = MagnetStrength * ForceMultiplier;

        for (auto It = OverlappingMetals.CreateIterator(); It; ++It)
        {
            UPrimitiveComponent *TargetComp = It->Get();
            if (!IsValid(TargetComp))
            {
                It.RemoveCurrent();
                continue;
            }

            AActor *OtherActor = TargetComp->GetOwner();
            if (!OtherActor)
            {
                It.RemoveCurrent();
                continue;
            }

            const bool bIsMetal = OtherActor->ActorHasTag(MetalTag);
            const bool bIsMagnet = OtherActor->ActorHasTag(MagnetTag);
            // ★ Copper는 여기서 제외됨 (OverlappingMetals에 안 들어감)
            if (!bIsMetal && !bIsMagnet)
            {
                It.RemoveCurrent();
                continue;
            }

            const FVector OtherLoc = OtherActor->GetActorLocation();
            const FVector ToOther = OtherLoc - MagnetLoc;
            const float Distance = ToOther.Size();
            if (Distance > MaxDistance || Distance < 1.f)
                continue;

            const FVector DirToOther = ToOther / Distance;
            const float SafeDist = FMath::Max(Distance, MinDistance);

            ATransformation_actor *OtherMag = bIsMagnet ? Cast<ATransformation_actor>(OtherActor) : nullptr;
            if (OtherMag)
            {
                if (bMagnetSnapped || OtherMag->bMagnetSnapped)
                    continue;
                if (reinterpret_cast<uintptr_t>(this) > reinterpret_cast<uintptr_t>(OtherMag))
                    continue;
                if (!OtherMag->MeshComp || !OtherMag->MeshComp->IsSimulatingPhysics())
                    continue;

                const float MyPole = FVector::DotProduct(MyNorth, DirToOther);
                const float OtherPole = FVector::DotProduct(OtherMag->GetNorthPoleWorldDir(), -DirToOther);
                const float Polarity = -(MyPole * OtherPole);

                float SpeedScale = FMath::Clamp(ReferenceDistance / FMath::Max(SafeDist, 1.f), 0.1f, 5.f);
                float Speed = MagnetApproachSpeed * SpeedScale * FMath::Abs(Polarity);
                FVector MoveDir = DirToOther * FMath::Sign(Polarity);

                const float MyMass = FMath::Max(MeshComp->GetMass(), 0.1f);
                const float OtMass = FMath::Max(OtherMag->MeshComp->GetMass(), 0.1f);
                const float TotMass = MyMass + OtMass;

                FVector MyVel = MoveDir * Speed * (OtMass / TotMass);
                MyVel.Z = MeshComp->GetPhysicsLinearVelocity().Z;
                MeshComp->SetPhysicsLinearVelocity(MyVel);

                FVector OtVel = -MoveDir * Speed * (MyMass / TotMass);
                OtVel.Z = OtherMag->MeshComp->GetPhysicsLinearVelocity().Z;
                OtherMag->MeshComp->SetPhysicsLinearVelocity(OtVel);

                if (Polarity > 0.f && Distance <= MagnetSnapDistance)
                {
                    bMagnetSnapped = true;
                    OtherMag->bMagnetSnapped = true;
                    MeshComp->SetPhysicsLinearVelocity(FVector::ZeroVector);
                    OtherMag->MeshComp->SetPhysicsLinearVelocity(FVector::ZeroVector);
                }
                continue;
            }

            // ── 자석-금속 (Metal만, Copper는 여기 안 옴) ──
            if (!TargetComp->IsSimulatingPhysics())
                continue;

            const FVector MetalLoc = TargetComp->GetComponentLocation();
            const FVector ToMagnet = MagnetLoc - MetalLoc;
            const float Dist = ToMagnet.Size();
            if (Dist < MinDistance || Dist > MaxDistance)
                continue;

            const FVector Dir = ToMagnet / Dist;
            const float DirDot = FVector::DotProduct(Dir, MagnetFwd);
            const float DirFactor = FMath::Lerp(0.75f, 1.0f, (DirDot + 1.0f) * 0.5f);

            float ForceMag = (StrMul * DirFactor) / FMath::Pow(FMath::Max(Dist, MinDistance), MagneticDecayExponent);
            ForceMag *= FMath::Clamp(TargetComp->GetMass() / 5.0f, 0.6f, 2.5f);

            const FVector CurVel = TargetComp->GetPhysicsLinearVelocity();
            const float VelToward = FVector::DotProduct(CurVel, Dir);
            float VelDamp = 1.0f;
            if (VelToward > MaxAttractVelocity * 0.7f)
                VelDamp = FMath::Clamp(1.0f - (VelToward / MaxAttractVelocity), 0.4f, 1.0f);

            FVector Force = (Dir * ForceMag * VelDamp) + (-CurVel * VelocityDampingFactor * TargetComp->GetMass());
            Force = Force.GetClampedToMaxSize(MaxForceClamp);

            TargetComp->AddForce(Force, NAME_None, false);

            if (bUseTorque)
            {
                const FVector Cross = FVector::CrossProduct(TargetComp->GetForwardVector(), Dir);
                const float TorqueMag = Cross.Size() * ForceMag * 0.3f;
                if (TorqueMag > 0.01f)
                    TargetComp->AddTorqueInRadians(Cross.GetSafeNormal() * TorqueMag, NAME_None, false);
            }

            if (bMagSim)
                MeshComp->AddForce(-Force * 0.2f, NAME_None, false);
        }

        if (bEnableInduction)
            ApplyInducedMagnetism();
    }
}

// ============================================================================
//  ★ RefreshOverlappingMetals — Copper를 별도 세트로 분류
// ============================================================================
void ATransformation_actor::RefreshOverlappingMetals()
{
    OverlappingMetals.Empty();
    OverlappingCoppers.Empty(); // ★
    if (!MeshComp)
        return;

    UWorld *World = GetWorld();
    if (!World)
        return;

    FCollisionQueryParams Q(SCENE_QUERY_STAT(MagnetSense), false);
    Q.AddIgnoredActor(this);

    TArray<FOverlapResult> Hits;
    World->OverlapMultiByObjectType(Hits, GetActorLocation(), FQuat::Identity,
                                    FCollisionObjectQueryParams::AllObjects,
                                    FCollisionShape::MakeSphere(MaxDistance), Q);

    for (const FOverlapResult &H : Hits)
    {
        UPrimitiveComponent *Comp = H.GetComponent();
        if (!Comp)
            continue;
        AActor *CompOwner = Comp->GetOwner();
        if (!CompOwner || CompOwner == this)
            continue;

        if (CompOwner->ActorHasTag(MetalTag) && Comp->IsSimulatingPhysics())
            OverlappingMetals.Add(Comp);
        else if (CompOwner->ActorHasTag(MagnetTag))
            OverlappingMetals.Add(Comp);
        // ★ Copper: 별도 세트에 추가 (반자성 척력 대상)
        else if (CompOwner->ActorHasTag(CopperTag) && Comp->IsSimulatingPhysics())
            OverlappingCoppers.Add(Comp);
    }
}

void ATransformation_actor::ClearPower()
{
    StoredVoltage = 0.f;
    StoredCurrent = 0.f;
    SetElectrified(false);
}

float ATransformation_actor::GetEffectiveVoltage() const
{
    // 이제 매번 계산하지 않고, 주입받아 저장된 값을 바로 뱉어냄
    return StoredVoltage;
}

float ATransformation_actor::GetEffectiveCurrent() const
{
    return StoredCurrent;
}

void ATransformation_actor::ApplyFixedPhysics()
{
    if (!MeshComp)
        return;
    MeshComp->SetSimulatePhysics(false);
    MeshComp->SetEnableGravity(false);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); // 충돌은 유지
}

void ATransformation_actor::SetGlobalMagneticCameraState(const UObject *WorldContextObject, bool bIsCameraOn)
{
    if (!WorldContextObject || !WorldContextObject->GetWorld())
        return;

    // 맵에 있는 모든 큐브를 자동으로 찾습니다.
    TArray<AActor *> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(WorldContextObject->GetWorld(), ATransformation_actor::StaticClass(), FoundActors);

    // 찾은 모든 큐브에게 카메라 켜짐/꺼짐 신호를 전달합니다.
    for (AActor *Actor : FoundActors)
    {
        if (ATransformation_actor *Cube = Cast<ATransformation_actor>(Actor))
        {
            Cube->SetMagneticCameraState(bIsCameraOn);
        }
    }
}

void ATransformation_actor::SetMagneticCameraState(bool bIsCameraOn)
{
    bIsMagneticCameraOn = bIsCameraOn;
    RefreshArrowVisibility();
}

void ATransformation_actor::RefreshArrowVisibility()
{
    if (SpawnedArrowEffect)
    {
        // 자기장 카메라 켜짐 + 현재 자석 폼 + 자력 안 잃음
        // 3가지 조건이 모두 맞아야만 화살표가 보임!
        bool bShouldShow = bIsMagneticCameraOn && (CurrentForm == EBlockForm::Magnet) && (!bDemagnetized);

        SpawnedArrowEffect->SetActorHiddenInGame(!bShouldShow);
    }
}

void ATransformation_actor::OnRubberHit(UPrimitiveComponent *HitComp, AActor *OtherActor,
                                        UPrimitiveComponent *OtherComp, FVector NormalImpulse, const FHitResult &Hit)
{
    if (CurrentForm != EBlockForm::Rubber)
        return;
    if (!OtherComp || !OtherComp->IsSimulatingPhysics())
        return;

    FVector Vel = OtherComp->GetPhysicsLinearVelocity();
    float Speed = FMath::Max(Vel.Size(), 300.f);

    FVector BounceDir = Hit.ImpactNormal;
    FVector NewVel;
    NewVel.X = Vel.X;
    NewVel.Y = Vel.Y;
    NewVel.Z = FMath::Abs(BounceDir.Z) * Speed * CurrentBounceMultiplier;

    OtherComp->SetPhysicsLinearVelocity(NewVel);

    CurrentBounceMultiplier = FMath::Max(CurrentBounceMultiplier * RubberBounceDecay, 0.1f);
}

void ATransformation_actor::UpdateSteamEffect()
{
    if (!MeshComp)
        return;

    if (bPendingDelayedDestroy)   // ★ 지연 파괴 중엔 손대지 않음
        return;

    if (IsActorBeingDestroyed())
    {
        DestroySteamEffect();
        return;
    }

    // ★ 지금 실제로 가열받는 중인지 확인
    bool bActivelyHeating = false;
    if (bHeating && CurrentFire)
    {
        const float DistCm = FVector::Dist(CurrentFire->GetActorLocation(), GetActorLocation());
        bActivelyHeating = (CalcReceivedPower(DistCm) > 0.f);
    }

    // 녹는 중(MeltAlpha>0.05) + 지금도 가열받는 중일 때만 수증기
    if (MeltAlpha > 0.05f && bActivelyHeating)
    {
        if (!SteamComponent)
            SpawnSteamEffect();

        if (SteamComponent)
{
    FVector LocalMin, LocalMax;
    MeshComp->GetLocalBounds(LocalMin, LocalMax);   // 스케일 미반영 로컬 크기
    // 큐브 윗면 중앙에 붙이기 (로컬 공간)
    SteamComponent->SetRelativeLocation(FVector(0.f, 0.f, LocalMax.Z));

    SteamComponent->SetWorldRotation(FRotator::ZeroRotator);
    SteamComponent->SetVariableFloat(FName(TEXT("SpawnRate")),
        FMath::Lerp(10.f, 100.f, MeltAlpha));
}
    }
    else
    {
        // 가열 안 받으면 수증기 끔
        DestroySteamEffect();
    }
}

void ATransformation_actor::SpawnSteamEffect()
{
    if (!SteamEffect || !MeshComp)
        return;
    if (SteamComponent)
        return;

    // ★ 액터가 죽는 중이거나 거의 다 녹았으면 수증기 새로 만들지 않음
    if (IsActorBeingDestroyed())
        return;

    const FVector BoxExtent = MeshComp->Bounds.BoxExtent;
    SteamComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
        SteamEffect, MeshComp, NAME_None,
        FVector(0.f, 0.f, BoxExtent.Z), FRotator::ZeroRotator,
        EAttachLocation::KeepRelativeOffset,
        false, true, ENCPoolMethod::None, true);
}

void ATransformation_actor::DestroySteamEffect()
{
    if (SteamComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT(">>> DestroySteamEffect 호출됨, 수증기 서서히 감쇠 시작"));
        
        // ★ Immediate 계열 함수를 절대 사용하지 않습니다!
        // 새로운 입자 생성을 중단하고 기존 입자는 유지하는 완만한 비활성화
        SteamComponent->Deactivate(); 
        
        // 월드에 이펙트 잔상이 남도록 분리
        SteamComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        
        SteamComponent = nullptr;
    }
}

void ATransformation_actor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 액터가 죽을 때 수증기를 '강제 파괴'하지 않고 감쇠 비활성화만 유도합니다.
    DestroySteamEffect();
    Super::EndPlay(EndPlayReason);
}

void ATransformation_actor::BeginDelayedDestroy()
{
    if (bPendingDelayedDestroy)
        return;
    bPendingDelayedDestroy = true;

    // 1. 죽는 순간 수증기가 없으면 즉석 생성
    if (!SteamComponent && SteamEffect && MeshComp)
    {
        const FVector BoxExtent = MeshComp->Bounds.BoxExtent;
        SteamComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
            SteamEffect, MeshComp, NAME_None,
            FVector(0.f, 0.f, BoxExtent.Z), FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset,
            false, true, ENCPoolMethod::None, true);
    }

    if (SteamComponent)
    {
        // 2. 수증기를 블럭 메쉬에서 분리
        SteamComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

        // 3. 다 녹기 직전의 연기 양을 그대로 유지
        SteamComponent->SetVariableFloat(FName(TEXT("SpawnRate")), 
            FMath::Lerp(10.f, 100.f, MeltAlpha));
    }

    // 4. 블럭 본체만 숨김 및 물리 차단
    if (MeshComp)
    {
        MeshComp->SetVisibility(false, false);
        MeshComp->SetSimulatePhysics(false);
        MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // 5. 실시간 상태 갱신(Tick) 정지
    SetActorTickEnabled(false);

    // ========================================================================
    // ★ 시간 조절 파트 (원하는 대로 초 단위 수정 가능)
    // ========================================================================
    
    // [타이머 1] 블럭이 사라진 후, 연기를 빵빵하게 '유지'할 시간 (기존 1.5초 -> 0.6초로 단축)
    const float SteamHoldTime = 0.6f; 

    GetWorld()->GetTimerManager().SetTimer(ResidualStopHandle,
        FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            if (SteamComponent)
            {
                // 유지 시간이 끝났으므로 새로운 연기 생성 중단 -> 남은 연기는 자연 소멸
                SteamComponent->Deactivate();
                SteamComponent = nullptr; 
            }
        }), SteamHoldTime, false);

    // [타이머 2] 연기가 스르륵 다 사라진 후 액터를 완전히 파괴할 시간
    // 유지 시간(0.6초) + 입자가 공중에서 사라지는 시간(약 1.4초) = 총 2.0초 뒤 파괴
    const float TotalDestroyDelay = SteamHoldTime + 1.4f; 

    GetWorld()->GetTimerManager().SetTimer(ResidualKillHandle,
        FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            Destroy(); 
        }), TotalDestroyDelay, false);
}