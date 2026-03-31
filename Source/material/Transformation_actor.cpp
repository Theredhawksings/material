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
#include "Transformation_actor.h"
#include "Wire.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SplineComponent.h"
#include "Components/MeshComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"

ATransformation_actor::ATransformation_actor()
{
    PrimaryActorTick.bCanEverTick = true;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    SetRootComponent(MeshComp);

    MeshComp->SetMobility(EComponentMobility::Movable);
    MeshComp->SetCollisionProfileName(TEXT("PhysicsActor"));
    MeshComp->SetGenerateOverlapEvents(true);

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> WoodMatFinder(TEXT("/Game/modeling/Texture/M_wood"));
    if (WoodMatFinder.Succeeded())
    {
        BurnMaterial = WoodMatFinder.Object;
    }

    static ConstructorHelpers::FClassFinder<AActor> ArrowBP(
        TEXT("/Game/modeling/Object/Arrow/Arrow_Effect"));
    if (ArrowBP.Succeeded())
    {
        ArrowEffectClass = ArrowBP.Class;
    }
}

void ATransformation_actor::BeginPlay()
{
    Super::BeginPlay();

    MeshComp->SetRenderCustomDepth(false);
    MeshComp->SetCustomDepthStencilValue(0);

    if (!CycleOrder.Contains(EBlockForm::Magnet))
    {
        CycleOrder.Add(EBlockForm::Magnet);
    }

    if (const FBlockFormSpec* Spec = FindSpec(CurrentForm))
    {
        ApplySpec(*Spec);
    }

    if (bAutoUpdateTags)
    {
        UpdateTagsForForm(CurrentForm);
    }

    if (CurrentForm == EBlockForm::Ice)
    {
        BaseScaleBeforeMelt = MeshComp->GetComponentScale();
        EnterIceMode();
    }

    if (CurrentForm == EBlockForm::Wood)
    {
        BaseScaleBeforeBurn = MeshComp->GetComponentScale();
        EnterWoodMode();
    }

    if (CurrentForm == EBlockForm::Magnet)
    {
        EnterMagnetMode();
    }

    GetWorld()->GetTimerManager().SetTimer(RefreshTimerHandle, this, &ATransformation_actor::RefreshConnectedWires, 0.2f, true);
}

void ATransformation_actor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (MeshComp && MeshComp->GetStaticMesh() == nullptr)
    {
        if (const FBlockFormSpec* Spec = FindSpec(CurrentForm))
        {
            ApplySpec(*Spec);
        }
        if (CurrentForm == EBlockForm::Ice)
        {
            EnterIceMode();
        }
        if (CurrentForm == EBlockForm::Wood)
        {
            EnterWoodMode();
        }
        if (CurrentForm == EBlockForm::Magnet)
        {
            EnterMagnetMode();
        }
    }
}

void ATransformation_actor::SetPowered(bool bNewPowered)
{
    if (CurrentForm != EBlockForm::Metal) return;
    if (bElectrified == bNewPowered) return;

    SetElectrified(bNewPowered);
    EnergizeWiresIfElectrified();
}

void ATransformation_actor::RefreshConnectedWires()
{
    if (CurrentForm != EBlockForm::Metal || !MeshComp)
    {
        for (auto It = WiresEnergizedByMetal.CreateIterator(); It; ++It)
        {
            if (AWire* W = It->Get())
            {
                W->SetPoweredByMetal(false);
            }
        }
        WiresEnergizedByMetal.Empty();
        ConnectedWires.Empty();

        if (bElectrified)
        {
            SetElectrified(false);
        }
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        ConnectedWires.Empty();
        SetElectrified(false);
        return;
    }

    const FVector Center = MeshComp->Bounds.Origin;
    const float Radius = FMath::Max(MeshComp->Bounds.SphereRadius + WireSenseExtraRadius, 5.f);

    FCollisionObjectQueryParams Obj = FCollisionObjectQueryParams::AllObjects;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(MetalWireSense), false);
    Q.AddIgnoredActor(this);

    TArray<FOverlapResult> Hits;
    World->OverlapMultiByObjectType(Hits, Center, FQuat::Identity, Obj, FCollisionShape::MakeSphere(Radius), Q);

    ConnectedWires.Empty();
    bool bAnyPowerFound = false;

    for (const FOverlapResult& H : Hits)
    {
        AWire* Wire = Cast<AWire>(H.GetActor());
        if (!Wire) continue;

        ConnectedWires.AddUnique(Wire);

        if (Wire->IsSourcePowered())
        {
    bAnyPowerFound = true;
        }
    }

    if (bElectrified != bAnyPowerFound)
    {
        SetElectrified(bAnyPowerFound);

        if (bElectrified)
        {
            EnergizeWiresIfElectrified();
        }
        else
        {
            for (auto It = WiresEnergizedByMetal.CreateIterator(); It; ++It)
            {
                if (AWire* W = It->Get()) W->SetPoweredByMetal(false);
            }
            WiresEnergizedByMetal.Empty();
        }
    }
}

void ATransformation_actor::SetElectrified(bool bNewElectrified)
{
    if (bElectrified == bNewElectrified) return;
    bElectrified = bNewElectrified;
}

void ATransformation_actor::EnergizeWiresIfElectrified()
{
    if (!bElectrified)
    {
        for (auto It = WiresEnergizedByMetal.CreateIterator(); It; ++It)
        {
            if (AWire* W = It->Get()) W->SetPoweredByMetal(false);
        }
        WiresEnergizedByMetal.Empty();
        return;
    }

    TSet<TWeakObjectPtr<AWire>> Current;
    for (AWire* Wire : ConnectedWires)
    {
        if (!Wire) continue;
        if (Wire->IsSourcePowered()) continue;

        Wire->SetPoweredByMetal(true);
        Wire->RefreshConnectedActors();

        Current.Add(Wire);
    }
    WiresEnergizedByMetal = MoveTemp(Current);
}

void ATransformation_actor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (CurrentForm == EBlockForm::Ice)
    {
        if (bHeating && CurrentFire && MeshComp && MeltAlpha < 1.0f)
        {
            const float DistCm = FVector::Dist(CurrentFire->GetActorLocation(), GetActorLocation());
            if (CurrentFire->MaxHeatDistance <= 0.0f || DistCm <= CurrentFire->MaxHeatDistance)
            {
                const float DistM = FMath::Max(DistCm / 100.0f, 0.05f);
                const float PtotalW = CurrentFire->GetTotalRadiantPowerW();
                float HeatFluxWm2 = PtotalW / (4.0f * PI * DistM * DistM);
                float ReceivedPowerW = HeatFluxWm2 * EffectiveAreaM2;

                if (CurrentFire->MaxHeatDistance > 0.0f)
                {
                    const float Fade = FMath::Clamp(1.0f - (DistCm / CurrentFire->MaxHeatDistance), 0.0f, 1.0f);
                    ReceivedPowerW *= Fade;
                }

                if (ReceivedPowerW > 0.0f)
                {
                    MeshComp->SetRenderCustomDepth(true);
                    EnergyAccumJ += ReceivedPowerW * DeltaTime * FMath::Max(SimTimeScale, 0.0f);
                    MeltAlpha = FMath::Clamp(EnergyAccumJ / FMath::Max(TotalMeltEnergyJ, 1.0f), 0.0f, 1.0f);
                    ApplyIceMeltVisual(MeltAlpha);

                    if (MeltAlpha >= 1.0f && bDestroyWhenMelted)
                    {
                        Destroy();
                    }
                }
            }
        }
    }

    if (CurrentForm == EBlockForm::Wood)
    {
        if (!bIsBurning)
        {
            if (bHeating && CurrentFire && MeshComp)
            {
                const float DistCm = FVector::Dist(CurrentFire->GetActorLocation(), GetActorLocation());

                if (CurrentFire->MaxHeatDistance <= 0.0f || DistCm <= CurrentFire->MaxHeatDistance)
                {
                    const float DistM = FMath::Max(DistCm / 100.0f, 0.05f);
                    const float PtotalW = CurrentFire->GetTotalRadiantPowerW();
                    float HeatFluxWm2 = PtotalW / (4.0f * PI * DistM * DistM);
                    float ReceivedPowerW = HeatFluxWm2 * EffectiveAreaM2;

                    if (CurrentFire->MaxHeatDistance > 0.0f)
                    {
                        const float Fade = FMath::Clamp(1.0f - (DistCm / CurrentFire->MaxHeatDistance), 0.0f, 1.0f);
                        ReceivedPowerW *= Fade;
                    }

                    const float MassKg = CurrentWoodMassKg;
                    const float DeltaT = (ReceivedPowerW * DeltaTime * WoodSimTimeScale) / (MassKg * SpecificHeatJPerKgK);
                    WoodTemperatureC += DeltaT;

                    MeshComp->SetRenderCustomDepth(true);
                    const float TempRatio = FMath::Clamp(WoodTemperatureC / WoodIgnitionTempC, 0.f, 1.f);
                    MeshComp->SetCustomDepthStencilValue(FMath::RoundToInt(TempRatio * 255.f));

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
            const float BurnedMassKg = BurnRateKgPerSec * DeltaTime;
            CurrentWoodMassKg -= BurnedMassKg;
            CurrentWoodMassKg = FMath::Max(CurrentWoodMassKg, 0.0f);

            BurnAlpha = 1.0f - (CurrentWoodMassKg / FMath::Max(WoodMassKg, 0.01f));

            MeshComp->SetRenderCustomDepth(true);
            MeshComp->SetCustomDepthStencilValue(255);

            ApplyWoodBurnVisual(BurnAlpha);

            if (CurrentWoodMassKg <= 0.0f)
            {
                if (bDestroyWhenBurned)
                {
                    Destroy();
                }
                else
                {
                    bIsBurning = false;
                    MeshComp->SetRenderCustomDepth(false);
                    MeshComp->SetCustomDepthStencilValue(0);
                }
            }
        }
    }

    if (CurrentForm == EBlockForm::Magnet)
    {
        UpdateMagnetism(DeltaTime);
    }
}

void ATransformation_actor::SetForm(EBlockForm NewForm)
{
    if (CurrentForm == NewForm)
    {
        if (const FBlockFormSpec* Spec = FindSpec(CurrentForm))
        {
            ApplySpec(*Spec);
        }
        return;
    }

    float SavedMeltAlpha = MeltAlpha;
    float SavedEnergyAccumJ = EnergyAccumJ;
    ATemperature* SavedFire = CurrentFire;
    bool bWasHeating = bHeating;
    FVector SavedCurrentScale = MeshComp ? MeshComp->GetComponentScale() : FVector(1, 1, 1);

    if (CurrentForm == EBlockForm::Ice)
    {
        ExitIceMode();
    }

    if (CurrentForm == EBlockForm::Wood)
    {
        ExitWoodMode();
    }

    if (CurrentForm == EBlockForm::Metal)
    {
        SetElectrified(false);
        for (auto It = WiresEnergizedByMetal.CreateIterator(); It; ++It)
        {
            if (AWire* W = It->Get()) W->SetPoweredByMetal(false);
        }
        WiresEnergizedByMetal.Empty();
        ConnectedWires.Empty();
    }

    if (CurrentForm == EBlockForm::Magnet)
    {
        ExitMagnetMode();
    }

    CurrentForm = NewForm;

    if (bAutoUpdateTags)
    {
        UpdateTagsForForm(NewForm);
    }

    if (const FBlockFormSpec* Spec = FindSpec(CurrentForm))
    {
        ApplySpec(*Spec);
    }

    MeshComp->SetRenderCustomDepth(false);
    MeshComp->SetCustomDepthStencilValue(0);

    if (MeshComp && SavedMeltAlpha > 0.0f)
    {
        MeshComp->SetWorldScale3D(SavedCurrentScale);
    }

    if (CurrentForm == EBlockForm::Ice)
    {
        BaseScaleBeforeMelt = SavedCurrentScale;
        EnterIceMode();
        MeltAlpha = SavedMeltAlpha;
        EnergyAccumJ = SavedEnergyAccumJ;
        CurrentFire = SavedFire;
        bHeating = bWasHeating && (CurrentFire != nullptr);
        ApplyIceMeltVisual(MeltAlpha);
    }

    if (CurrentForm == EBlockForm::Wood)
    {
        BaseScaleBeforeBurn = SavedCurrentScale;
        EnterWoodMode();
        CurrentFire = SavedFire;
        bHeating = bWasHeating && (CurrentFire != nullptr);
    }

    if (CurrentForm == EBlockForm::Magnet)
    {
        EnterMagnetMode();
    }
}

void ATransformation_actor::DecreaseGaugeForCurrentTag()
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;

    AmaterialCharacter* PlayerChar = Cast<AmaterialCharacter>(PC->GetPawn());
    if (!PlayerChar) return;

    if (ActorHasTag(TEXT("Rubber")))
        PlayerChar->DecreaseGaugeForMaterial(TEXT("Rubber"));
    else if (ActorHasTag(TEXT("Metal")))
        PlayerChar->DecreaseGaugeForMaterial(TEXT("Metal"));
    else if (ActorHasTag(TEXT("Ice")))
        PlayerChar->DecreaseGaugeForMaterial(TEXT("Ice"));
    else if (ActorHasTag(TEXT("Wood")))
        PlayerChar->DecreaseGaugeForMaterial(TEXT("Wood"));
    else if (ActorHasTag(TEXT("Magnet")))
        PlayerChar->DecreaseGaugeForMaterial(TEXT("Magnet"));
}

void ATransformation_actor::NextForm()
{
    if (CycleOrder.Num() <= 0) return;

    int32 Idx = CycleOrder.Find(CurrentForm);

    if (Idx == INDEX_NONE)
    {
        SetForm(CycleOrder[0]);
        DecreaseGaugeForCurrentTag();
        return;
    }

    Idx = (Idx + 1) % CycleOrder.Num();
    SetForm(CycleOrder[Idx]);
    DecreaseGaugeForCurrentTag();
}

void ATransformation_actor::UpdateTagsForForm(EBlockForm Form)
{
    ClearAllFormTags();
    switch (Form)
    {
    case EBlockForm::Ice:    Tags.AddUnique(IceTag);    break;
    case EBlockForm::Metal:  Tags.AddUnique(MetalTag);  break;
    case EBlockForm::Wood:   Tags.AddUnique(WoodTag);   break;
    case EBlockForm::Rubber: Tags.AddUnique(RubberTag); break;
    case EBlockForm::Magnet: Tags.AddUnique(MagnetTag); break;
    default: break;
    }
}

void ATransformation_actor::ClearAllFormTags()
{
    Tags.Remove(IceTag);
    Tags.Remove(MetalTag);
    Tags.Remove(WoodTag);
    Tags.Remove(RubberTag);
    Tags.Remove(MagnetTag);
}

void ATransformation_actor::StartHeating(ATemperature* FireRef)
{
    CurrentFire = FireRef;
    bHeating = (CurrentFire != nullptr);

    if (CurrentForm != EBlockForm::Ice && CurrentForm != EBlockForm::Wood)
    {
        bHeating = false;
    }
}

void ATransformation_actor::ReceiveHeatEnergy(float EnergyJ, float SourceTempC)
{
    if (CurrentForm != EBlockForm::Ice) return;
    if (!MeshComp) return;
    if (EnergyJ <= 0.f) return;

    MeshComp->SetRenderCustomDepth(true);

    EnergyAccumJ += EnergyJ * FMath::Max(SimTimeScale, 0.0f);
    MeltAlpha = FMath::Clamp(EnergyAccumJ / FMath::Max(TotalMeltEnergyJ, 1.0f), 0.0f, 1.0f);
    ApplyIceMeltVisual(MeltAlpha);

    if (MeltAlpha >= 1.0f && bDestroyWhenMelted)
    {
        Destroy();
    }
}

void ATransformation_actor::StopHeating()
{
    bHeating = false;
    CurrentFire = nullptr;

    if (MeshComp)
    {
        if (CurrentForm == EBlockForm::Wood && bIsBurning)
        {
            return;
        }
        MeshComp->SetRenderCustomDepth(false);
        MeshComp->SetCustomDepthStencilValue(0);
    }
}

const FBlockFormSpec* ATransformation_actor::FindSpec(EBlockForm Form) const
{
    for (const FBlockFormSpec& S : FormSpecs)
    {
        if (S.Form == Form) return &S;
    }
    return nullptr;
}

void ATransformation_actor::ApplySpec(const FBlockFormSpec& Spec)
{
    if (!MeshComp) return;
    if (Spec.Mesh) MeshComp->SetStaticMesh(Spec.Mesh);
    if (Spec.Materials.Num() > 0)
    {
        for (int32 i = 0; i < Spec.Materials.Num(); ++i)
        {
            if (Spec.Materials[i]) MeshComp->SetMaterial(i, Spec.Materials[i]);
        }
    }
    MeshComp->SetSimulatePhysics(Spec.bSimulatePhysics);
    MeshComp->SetLinearDamping(Spec.LinearDamping);
    MeshComp->SetAngularDamping(Spec.AngularDamping);
    if (Spec.PhysMat) MeshComp->SetPhysMaterialOverride(Spec.PhysMat);
    if (Spec.bOverrideMass) MeshComp->SetMassOverrideInKg(NAME_None, Spec.MassKg, true);
}

void ATransformation_actor::EnterIceMode()
{
    if (!MeshComp) return;
    if (MeltAlpha == 0.0f) EnergyAccumJ = 0.0f;
    RecalcIceMassAndEnergy();
    IceMID = nullptr;

    if (IceMeltMaterial)
    {
        IceMID = UMaterialInstanceDynamic::Create(IceMeltMaterial, this);
        if (IceMID) MeshComp->SetMaterial(0, IceMID);
    }
    else
    {
        UMaterialInterface* M0 = MeshComp->GetMaterial(0);
        if (M0)
        {
            IceMID = UMaterialInstanceDynamic::Create(M0, this);
            if (IceMID) MeshComp->SetMaterial(0, IceMID);
        }
    }
}

void ATransformation_actor::ExitIceMode()
{
    IceMID = nullptr;
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

    const FBoxSphereBounds LocalBounds = MeshComp->GetStaticMesh()->GetBounds();
    const FVector SafeBaseScale = FVector(
        FMath::Max(BaseScaleBeforeMelt.X, 0.01f),
        FMath::Max(BaseScaleBeforeMelt.Y, 0.01f),
        FMath::Max(BaseScaleBeforeMelt.Z, 0.01f)
    );
    const FVector OriginalSizeCm = LocalBounds.BoxExtent * 2.0f * SafeBaseScale;
    const FVector SizeM = OriginalSizeCm / 100.0f;
    VolumeM3 = FMath::Max(SizeM.X * SizeM.Y * SizeM.Z, 1e-6f);
    const float Axy = SizeM.X * SizeM.Y;
    const float Axz = SizeM.X * SizeM.Z;
    const float Ayz = SizeM.Y * SizeM.Z;
    EffectiveAreaM2 = FMath::Max3(Axy, Axz, Ayz);
    const float MassKg = IceDensityKgM3 * VolumeM3;
    TotalMeltEnergyJ = FMath::Max(MassKg * LatentHeatJPerKg, 1.0f);
}

void ATransformation_actor::ApplyIceMeltVisual(float Alpha01)
{
    if (!MeshComp) return;

    const float A = FMath::Clamp(Alpha01, 0.0f, 1.0f);
    const float Ratio = FMath::Clamp(MinScaleRatio, 0.0f, 1.0f);
    const FVector From = BaseScaleBeforeMelt;
    const FVector To = BaseScaleBeforeMelt * Ratio;
    const FVector NewScale = FMath::Lerp(From, To, A);
    MeshComp->SetWorldScale3D(NewScale);

    if (IceMID)
    {
        IceMID->SetScalarParameterValue(MeltParamName, A);
    }

    if (NewScale.X <= MinScaleRatio && NewScale.Y <= MinScaleRatio && NewScale.Z <= MinScaleRatio)
    {
        Destroy();
    }
}

void ATransformation_actor::EnterWoodMode()
{
    if (!MeshComp) return;

    RecalcWoodMassAndVolume();

    WoodTemperatureC = 20.0f;
    CurrentWoodMassKg = WoodMassKg;
    BurnAlpha = 0.0f;
    bIsBurning = false;

    BurnMID = nullptr;

    if (BurnMaterial)
    {
        BurnMID = UMaterialInstanceDynamic::Create(BurnMaterial, this);
        if (BurnMID)
        {
            MeshComp->SetMaterial(0, BurnMID);
        }
    }
    else
    {
        UMaterialInterface* M0 = MeshComp->GetMaterial(0);
        if (M0)
        {
            BurnMID = UMaterialInstanceDynamic::Create(M0, this);
            if (BurnMID) MeshComp->SetMaterial(0, BurnMID);
        }
    }

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

    const FBoxSphereBounds LocalBounds = MeshComp->GetStaticMesh()->GetBounds();
    const FVector SafeBaseScale = FVector(
        FMath::Max(BaseScaleBeforeBurn.X, 0.01f),
        FMath::Max(BaseScaleBeforeBurn.Y, 0.01f),
        FMath::Max(BaseScaleBeforeBurn.Z, 0.01f)
    );
    const FVector SizeCm = LocalBounds.BoxExtent * 2.0f * SafeBaseScale;
    const FVector SizeM = SizeCm / 100.0f;

    WoodVolumeM3 = FMath::Max(SizeM.X * SizeM.Y * SizeM.Z, 1e-6f);
    WoodMassKg = WoodDensityKgM3 * WoodVolumeM3;

    const float Axy = SizeM.X * SizeM.Y;
    const float Axz = SizeM.X * SizeM.Z;
    const float Ayz = SizeM.Y * SizeM.Z;
    EffectiveAreaM2 = FMath::Max3(Axy, Axz, Ayz);
}

void ATransformation_actor::ApplyWoodBurnVisual(float Alpha01)
{
    if (!MeshComp) return;

    const float A = FMath::Clamp(Alpha01, 0.0f, 1.0f);

    if (BurnMID)
    {
        BurnMID->SetScalarParameterValue(BurnParamName, A);
    }

    const float MassRatio = (1.0f - A);
    const float VolumeRatio = FMath::Max(MassRatio, MinBurnScaleRatio);
    const float ScaleRatio = FMath::Pow(VolumeRatio, 1.0f / 3.0f);

    const FVector NewScale = BaseScaleBeforeBurn * ScaleRatio;
    MeshComp->SetWorldScale3D(NewScale);
}

FVector ATransformation_actor::GetNorthPoleWorldDir() const
{
    if (!MeshComp) return FVector::ForwardVector;
    return MeshComp->GetComponentTransform().TransformVectorNoScale(NorthPoleLocalDir).GetSafeNormal();
}

FVector ATransformation_actor::GetSouthPoleWorldDir() const
{
    return -GetNorthPoleWorldDir();
}

// ============================================================================
//  [추가 1] CheckDemagnetize — 열원 근처에서 자력 상실
// ============================================================================
void ATransformation_actor::CheckDemagnetize()
{
    if (bDemagnetized) return;

    TArray<AActor*> HeatSources;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATemperature::StaticClass(), HeatSources);

    const FVector MyLoc = GetActorLocation();

    for (AActor* Actor : HeatSources)
    {
        const ATemperature* Heat = Cast<ATemperature>(Actor);
        if (!Heat) continue;

        const float DistCm = FVector::Dist(MyLoc, Heat->GetActorLocation());

        if (Heat->MaxHeatDistance > 0.f && DistCm <= Heat->MaxHeatDistance)
        {
            bDemagnetized = true;
            bElectroActive = false;
            MagnetStrength = 0.f;
            OverlappingMetals.Empty();
            MagnetContactedWires.Empty();
            PreviousOverlappingMetals.Empty();

            if (SpawnedArrowEffect)
            {
                SpawnedArrowEffect->Destroy();
                SpawnedArrowEffect = nullptr;
            }

#if ENABLE_DRAW_DEBUG
            if (bDebugDraw)
            {
                DrawDebugString(GetWorld(), MyLoc + FVector(0, 0, 100),
                    TEXT("DEMAGNETIZED"), nullptr, FColor::Red, 5.0f, true);
            }
#endif
            return;
        }
    }
}

// ============================================================================
//  [추가 2] UpdateMagnetElectroBoost — 전선 접촉 시 전자석 부스트
// ============================================================================
void ATransformation_actor::UpdateMagnetElectroBoost()
{
    bool bAnyPowered = false;
    float TotalCurrent = 0.f;

    TArray<AActor*> NearbyActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWire::StaticClass(), NearbyActors);

    MagnetContactedWires.Empty();

    const FVector MyLoc = GetActorLocation();

    for (AActor* Actor : NearbyActors)
    {
        AWire* Wire = Cast<AWire>(Actor);
        if (!Wire) continue;
        if (!Wire->IsPowered()) continue;

        bool bClose = false;

        USplineComponent* WireSpline = Wire->GetSplineComponent();
        if (WireSpline)
        {
            const FVector Closest = WireSpline->FindLocationClosestToWorldLocation(MyLoc, ESplineCoordinateSpace::World);
            if (FVector::Dist(MyLoc, Closest) <= WireContactRadius)
            {
                bClose = true;
            }

            if (!bClose)
            {
                const int32 NumPoints = WireSpline->GetNumberOfSplinePoints();
                for (int32 i = 0; i < NumPoints; ++i)
                {
                    const FVector PointWorld = WireSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
                    if (FVector::Dist(MyLoc, PointWorld) <= WireContactRadius)
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

    if (bElectroActive)
    {
        const float CurrentBoost = FMath::Clamp(TotalCurrent, 1.0f, ElectroBoostMultiplier);
        MagnetStrength = BaseMagnetStrength * CurrentBoost;
    }
    else
    {
        MagnetStrength = BaseMagnetStrength;
    }
}

// ============================================================================
//  [추가 3] ApplyInducedMagnetism — 금속 간 유도 자기
// ============================================================================
void ATransformation_actor::ApplyInducedMagnetism()
{
    const FVector MagnetLoc = GetActorLocation();
    const TArray<UPrimitiveComponent*> MetalArray = OverlappingMetals.Array();
    const int32 Num = MetalArray.Num();

    for (int32 i = 0; i < Num; ++i)
    {
        UPrimitiveComponent* MetalA = MetalArray[i];
        if (!IsValid(MetalA) || !MetalA->IsSimulatingPhysics()) continue;

        const FVector MetalALoc = MetalA->GetComponentLocation();
        const float DistAToMagnet = FVector::Dist(MetalALoc, MagnetLoc);

        if (DistAToMagnet > MinDistanceForInduction) continue;

        const float InducedStrength = CalculateInducedStrength(DistAToMagnet, MagnetStrength);
        const FVector MagnetToA = (MetalALoc - MagnetLoc).GetSafeNormal();

        for (int32 j = i + 1; j < Num; ++j)
        {
            UPrimitiveComponent* MetalB = MetalArray[j];
            if (!IsValid(MetalB) || !MetalB->IsSimulatingPhysics()) continue;

            const FVector MetalBLoc = MetalB->GetComponentLocation();
            const FVector AtoB = MetalBLoc - MetalALoc;
            const float DistAtoB = AtoB.Size();

            if (DistAtoB < 10.f || DistAtoB > InductionRange) continue;

            const FVector Dir = AtoB / DistAtoB;
            const float AlignmentFactor = FVector::DotProduct(Dir, MagnetToA);

            float ForceMag = (InducedStrength * InductionStrengthRatio * FMath::Abs(AlignmentFactor))
                        / FMath::Pow(DistAtoB, MagneticDecayExponent);

            const float MetalBMass = MetalB->GetMass();
            ForceMag *= FMath::Clamp(MetalBMass / 10.0f, 0.5f, 2.0f);

            const FVector CurrentVelB = MetalB->GetPhysicsLinearVelocity();
            const float VelTowardsA = FVector::DotProduct(CurrentVelB, Dir);

            float VelDamping = 1.0f;
            if (VelTowardsA > MaxAttractVelocity * 0.5f)
            {
                VelDamping = FMath::Clamp(1.0f - (VelTowardsA / MaxAttractVelocity), 0.3f, 1.0f);
            }

            const FVector DampingForce = -CurrentVelB * (VelocityDampingFactor * 0.5f * MetalBMass);
            FVector FinalForce = (Dir * ForceMag * VelDamping * AlignmentFactor) + DampingForce;
            FinalForce = FinalForce.GetClampedToMaxSize(MaxInducedForceClamp);

            MetalB->AddForce(FinalForce, NAME_None, false);
            MetalA->AddForce(-FinalForce * 0.5f, NAME_None, false);

#if ENABLE_DRAW_DEBUG
            if (bDebugDraw)
            {
                DrawDebugLine(GetWorld(), MetalALoc, MetalBLoc, FColor::Yellow, false, -1.f, 0, 1.f);
            }
#endif
        }
    }
}

// ============================================================================
//  [추가 4] CalculateInducedStrength
// ============================================================================
float ATransformation_actor::CalculateInducedStrength(float DistanceToMagnet, float BaseMagnetStrengthVal) const
{
    const float SafeDist = FMath::Max(DistanceToMagnet, 1.0f);
    const float InductionFactor = FMath::Clamp(
        1.0f / FMath::Pow(SafeDist / MinDistanceForInduction, 1.5f), 0.0f, 1.0f);
    return BaseMagnetStrengthVal * InductionFactor;
}

// ============================================================================
//  EnterMagnetMode
// ============================================================================
void ATransformation_actor::EnterMagnetMode()
{
    if (!MeshComp) return;

    bMagnetCollided = false;
    bMagnetSnapped = false;

    if (bAutoComputeStrength)
    {
        MagnetStrength = MaxLiftMass * GravityAccel * FMath::Pow(ReferenceDistance, MagneticDecayExponent);
    }

    BaseMagnetStrength = MagnetStrength;
    bDemagnetized = false;
    bElectroActive = false;
    TimeSinceLastMagnetRefresh = 0.f;

    OverlappingMetals.Empty();
    MagnetContactedWires.Empty();
    PreviousOverlappingMetals.Empty();

    MeshComp->SetSimulatePhysics(true);
    MeshComp->SetPhysicsLinearVelocity(FVector::ZeroVector);
    MeshComp->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    // 초기 소자 체크
    CheckDemagnetize();

    if (!bDemagnetized)
    {
        RefreshOverlappingMetals();
    }

    // ── 화살표 이펙트 스폰 ──
    if (bShowFieldArrows && !bDemagnetized && ArrowEffectClass)
    {
        FTimerHandle ArrowSpawnTimer;
        GetWorldTimerManager().SetTimer(ArrowSpawnTimer, [this]()
        {
            if (!IsValid(this) || !ArrowEffectClass) return;
            if (CurrentForm != EBlockForm::Magnet) return;
            if (bDemagnetized) return;

            FQuat ActorQuat = GetActorRotation().Quaternion();
            FQuat OffsetQuat = FRotator(90.f, 0.f, 0.f).Quaternion();
            FTransform SpawnTransform((ActorQuat * OffsetQuat).Rotator(), GetActorLocation());

            AActor* Arrow = GetWorld()->SpawnActorDeferred<AActor>(
                ArrowEffectClass, SpawnTransform, this, nullptr,
                ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

            if (Arrow)
            {
                FProperty* PowerProp = Arrow->GetClass()->FindPropertyByName(TEXT("Power"));
                FProperty* XProp     = Arrow->GetClass()->FindPropertyByName(TEXT("X"));
                FProperty* YProp     = Arrow->GetClass()->FindPropertyByName(TEXT("Y"));

                if (PowerProp)
                {
                    if (FDoubleProperty* D = CastField<FDoubleProperty>(PowerProp))
                        D->SetPropertyValue_InContainer(Arrow, (double)ArrowPower);
                    else if (FFloatProperty* F = CastField<FFloatProperty>(PowerProp))
                        F->SetPropertyValue_InContainer(Arrow, ArrowPower);
                }
                if (XProp)
                {
                    if (FDoubleProperty* D = CastField<FDoubleProperty>(XProp))
                        D->SetPropertyValue_InContainer(Arrow, (double)ArrowX);
                    else if (FFloatProperty* F = CastField<FFloatProperty>(XProp))
                        F->SetPropertyValue_InContainer(Arrow, ArrowX);
                }
                if (YProp)
                {
                    if (FDoubleProperty* D = CastField<FDoubleProperty>(YProp))
                        D->SetPropertyValue_InContainer(Arrow, (double)ArrowY);
                    else if (FFloatProperty* F = CastField<FFloatProperty>(YProp))
                        F->SetPropertyValue_InContainer(Arrow, ArrowY);
                }

                UGameplayStatics::FinishSpawningActor(Arrow, SpawnTransform);

                TArray<USceneComponent*> AllComps;
                Arrow->GetRootComponent()->GetChildrenComponents(true, AllComps);
                AllComps.Add(Arrow->GetRootComponent());
                for (USceneComponent* Comp : AllComps)
                {
                    Comp->SetMobility(EComponentMobility::Movable);
                }

                SpawnedArrowEffect = Arrow;
                SpawnedArrowEffect->SetActorHiddenInGame(true);
            }
        }, 1.0f, false);
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

    OverlappingMetals.Empty();
    MagnetContactedWires.Empty();
    PreviousOverlappingMetals.Empty();

    if (SpawnedArrowEffect)
    {
        SpawnedArrowEffect->Destroy();
        SpawnedArrowEffect = nullptr;
    }
}

void ATransformation_actor::OnMagnetHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    return;
}

// ============================================================================
//  UpdateMagnetism — 누락 기능 모두 통합
// ============================================================================
void ATransformation_actor::UpdateMagnetism(float DeltaTime)
{
    if (bDemagnetized || !MeshComp) return;
    if (MeshComp->GetCollisionEnabled() == ECollisionEnabled::NoCollision) return;

    TimeSinceLastMagnetRefresh += DeltaTime;
    if (TimeSinceLastMagnetRefresh >= MagnetRefreshInterval)
    {
        TimeSinceLastMagnetRefresh = 0.f;

        // [추가] 소자 체크
        CheckDemagnetize();
        if (bDemagnetized)
        {
            OverlappingMetals.Empty();
            PreviousOverlappingMetals.Empty();
            return;
        }

        // [추가] 전자석 부스트
        UpdateMagnetElectroBoost();

        RefreshOverlappingMetals();
    }

#if ENABLE_DRAW_DEBUG
    if (bDebugDraw)
    {
        if (bElectroActive)
        {
            DrawDebugSphere(GetWorld(), GetActorLocation(), WireContactRadius, 16, FColor::Cyan, false, -1.f, 0, 2.f);
            const float BoostRatio = (BaseMagnetStrength > 0.f) ? (MagnetStrength / BaseMagnetStrength) : 1.f;
            DrawDebugString(GetWorld(), GetActorLocation() + FVector(0, 0, 150),
                FString::Printf(TEXT("[전자석 활성]\n연결 전선: %d개\n기본 자력: %.0f\n현재 자력: %.0f\n부스트: x%.2f"),
                    MagnetContactedWires.Num(), BaseMagnetStrength, MagnetStrength, BoostRatio),
                nullptr, FColor::Cyan, 0.0f, true);
        }
        else
        {
            DrawDebugString(GetWorld(), GetActorLocation() + FVector(0, 0, 150),
                FString::Printf(TEXT("[자석 모드]\n자력: %.0f"), MagnetStrength),
                nullptr, FColor::White, 0.0f, true);
        }
    }
#endif

    if (OverlappingMetals.Num() == 0) return;

    // ── [추가] InitialImpulse: 새로 범위에 들어온 금속에 초기 임펄스 ──
    if (bApplyInitialImpulse)
    {
        for (UPrimitiveComponent* Comp : OverlappingMetals)
        {
            if (!IsValid(Comp) || !Comp->IsSimulatingPhysics()) continue;

            AActor* OwnerActor = Comp->GetOwner();
            if (!OwnerActor || !OwnerActor->ActorHasTag(MetalTag)) continue;

            // 이전 프레임에 없었으면 새로 진입
            if (!PreviousOverlappingMetals.Contains(Comp))
            {
                const FVector ToMagnet =
                    (GetActorLocation() - Comp->GetComponentLocation()).GetSafeNormal();
                Comp->AddImpulse(ToMagnet * InitialImpulseStrength * Comp->GetMass());
            }
        }
    }
    // 현재 목록을 저장해두고 다음 프레임과 비교
    PreviousOverlappingMetals = OverlappingMetals;

    const FVector MagnetLoc = GetActorLocation();
    const FVector MyNorth = GetNorthPoleWorldDir();
    const FVector MagnetForward = MeshComp->GetForwardVector();
    const bool bMagnetSimulating = MeshComp->IsSimulatingPhysics();
    const float StrengthTimesMultiplier = MagnetStrength * ForceMultiplier;

    // 유효하지 않은 항목 제거
    for (auto It = OverlappingMetals.CreateIterator(); It; ++It)
    {
        UPrimitiveComponent* Comp = It->Get();
        if (!IsValid(Comp))
        {
            It.RemoveCurrent();
            continue;
        }
        AActor* OwnerActor = Comp->GetOwner();
        if (!OwnerActor)
        {
            It.RemoveCurrent();
            continue;
        }
        if (!OwnerActor->ActorHasTag(MetalTag) && !OwnerActor->ActorHasTag(MagnetTag))
        {
            It.RemoveCurrent();
            continue;
        }
    }

    if (OverlappingMetals.Num() == 0) return;

    for (UPrimitiveComponent* TargetComp : OverlappingMetals)
    {
        if (!IsValid(TargetComp)) continue;

        AActor* OtherActor = TargetComp->GetOwner();
        if (!OtherActor) continue;

        const FVector OtherLoc = OtherActor->GetActorLocation();
        const FVector ToOther = OtherLoc - MagnetLoc;
        const float Distance = ToOther.Size();

        if (Distance > MaxDistance || Distance < 1.f) continue;

        const FVector DirToOther = ToOther / Distance;
        const float SafeDist = FMath::Max(Distance, MinDistance);

        ATransformation_actor* OtherMagnetActor = nullptr;
        if (OtherActor->ActorHasTag(MagnetTag))
        {
            OtherMagnetActor = Cast<ATransformation_actor>(OtherActor);
        }

        // ── 자석-자석 상호작용 (기존 로직 유지) ──
        if (OtherMagnetActor)
        {
            if (bMagnetSnapped || OtherMagnetActor->bMagnetSnapped) continue;

            if (reinterpret_cast<uintptr_t>(this) > reinterpret_cast<uintptr_t>(OtherMagnetActor))
                continue;

            if (!OtherMagnetActor->MeshComp || !OtherMagnetActor->MeshComp->IsSimulatingPhysics())
                continue;

            const FVector OtherNorth = OtherMagnetActor->GetNorthPoleWorldDir();

            const float MyPoleToward = FVector::DotProduct(MyNorth, DirToOther);
            const float OtherPoleToward = FVector::DotProduct(OtherNorth, -DirToOther);
            const float PolarityFactor = -(MyPoleToward * OtherPoleToward);

            float SpeedScale = (ReferenceDistance / FMath::Max(SafeDist, 1.f));
            SpeedScale = FMath::Clamp(SpeedScale, 0.1f, 5.f);

            float Speed = MagnetApproachSpeed * SpeedScale * FMath::Abs(PolarityFactor);

            FVector MoveDir = DirToOther * FMath::Sign(PolarityFactor);

            const float MyMass = FMath::Max(MeshComp->GetMass(), 0.1f);
            const float OtherMass = FMath::Max(OtherMagnetActor->MeshComp->GetMass(), 0.1f);
            const float TotalMass = MyMass + OtherMass;

            const float MySpeed = Speed * (OtherMass / TotalMass);
            const float OtherSpeed = Speed * (MyMass / TotalMass);

            const FVector MyCurrentVel = MeshComp->GetPhysicsLinearVelocity();
            FVector MyNewVel = MoveDir * MySpeed;
            MyNewVel.Z = MyCurrentVel.Z;
            MeshComp->SetPhysicsLinearVelocity(MyNewVel);

            const FVector OtherCurrentVel = OtherMagnetActor->MeshComp->GetPhysicsLinearVelocity();
            FVector OtherNewVel = -MoveDir * OtherSpeed;
            OtherNewVel.Z = OtherCurrentVel.Z;
            OtherMagnetActor->MeshComp->SetPhysicsLinearVelocity(OtherNewVel);

            if (PolarityFactor > 0.f && Distance <= MagnetSnapDistance)
            {
                bMagnetSnapped = true;
                OtherMagnetActor->bMagnetSnapped = true;

                MeshComp->SetPhysicsLinearVelocity(FVector::ZeroVector);
                OtherMagnetActor->MeshComp->SetPhysicsLinearVelocity(FVector::ZeroVector);
            }

            if (bDebugDraw)
            {
                FColor DebugColor = (PolarityFactor > 0) ? FColor::Green : FColor::Red;
                DrawDebugLine(GetWorld(), MagnetLoc, OtherLoc, DebugColor, false, 0.1f, 0, 2.f);
                DrawDebugString(GetWorld(), MagnetLoc + FVector(0, 0, 50),
                    FString::Printf(TEXT("P=%.2f D=%.0f MyM=%.1f OtM=%.1f"),
                        PolarityFactor, Distance, MyMass, OtherMass),
                    nullptr, FColor::White, 0.1f);
            }
        }
        // ── 자석-금속 상호작용 (누락 기능 모두 추가) ──
        else
        {
            if (!TargetComp->IsSimulatingPhysics()) continue;

            const FVector MetalLoc = TargetComp->GetComponentLocation();
            const FVector ToMagnet = MagnetLoc - MetalLoc;
            const float Dist = ToMagnet.Size();

            if (Dist < MinDistance || Dist > MaxDistance) continue;

            const FVector Dir = ToMagnet / Dist;

            // [추가] DirectionFactor — 자석 전방 방향 보정
            const float DirDot = FVector::DotProduct(Dir, MagnetForward);
            const float DirectionFactor = FMath::Lerp(0.75f, 1.0f, (DirDot + 1.0f) * 0.5f);

            const float MetalSafeDist = FMath::Max(Dist, MinDistance);
            float ForceMag = (StrengthTimesMultiplier * DirectionFactor)
                           / FMath::Pow(MetalSafeDist, MagneticDecayExponent);

            const float MetalMass = TargetComp->GetMass();
            ForceMag *= FMath::Clamp(MetalMass / 5.0f, 0.6f, 2.5f);

            // [추가] VelocityDamping — 접근 속도 제한
            const FVector CurrentVel = TargetComp->GetPhysicsLinearVelocity();
            const float VelTowardsMagnet = FVector::DotProduct(CurrentVel, Dir);

            float VelocityDamping = 1.0f;
            if (VelTowardsMagnet > MaxAttractVelocity * 0.7f)
            {
                VelocityDamping = FMath::Clamp(1.0f - (VelTowardsMagnet / MaxAttractVelocity), 0.4f, 1.0f);
            }

            const FVector DampingForce = -CurrentVel * (VelocityDampingFactor * MetalMass);
            FVector FinalForce = (Dir * ForceMag * VelocityDamping) + DampingForce;
            FinalForce = FinalForce.GetClampedToMaxSize(MaxForceClamp);

            TargetComp->AddForce(FinalForce, NAME_None, false);

            // [추가] Torque — 금속을 자석 쪽으로 회전
            if (bUseTorque)
            {
                const FVector CrossProduct = FVector::CrossProduct(TargetComp->GetForwardVector(), Dir);
                const float TorqueMagnitude = CrossProduct.Size() * ForceMag * 0.3f;

                if (TorqueMagnitude > 0.01f)
                {
                    TargetComp->AddTorqueInRadians(CrossProduct.GetSafeNormal() * TorqueMagnitude, NAME_None, false);
                }
            }

            // [추가] 반작용력 — 자석 자체에도 반대 힘
            if (bMagnetSimulating)
            {
                MeshComp->AddForce(-FinalForce * 0.2f, NAME_None, false);
            }

#if ENABLE_DRAW_DEBUG
            if (bDebugDraw)
            {
                const FColor LineColor = bElectroActive ? FColor::Cyan : FColor::Blue;
                DrawDebugLine(GetWorld(), MetalLoc, MagnetLoc, LineColor, false, -1.f, 0, 2.f);
                DrawDebugSphere(GetWorld(), MetalLoc, 25.f, 8, FColor::Red, false, -1.f);
                DrawDebugString(GetWorld(), MetalLoc + FVector(0, 0, 50),
                    FString::Printf(TEXT("%.0f N"), FinalForce.Size()), nullptr, FColor::Yellow, 0.0f);
            }
#endif
        }
    } // for 끝

    // [추가] 유도 자기
    if (bEnableInduction)
    {
        ApplyInducedMagnetism();
    }

    // ── 화살표 위치 동기화 ──
    if (SpawnedArrowEffect)
    {
        const FQuat OffsetQuat = FRotator(90.f, 0.f, 0.f).Quaternion();
        const FQuat DesiredQuat = GetActorQuat() * OffsetQuat;
        const FVector DesiredLoc = GetActorLocation();

        if (!SpawnedArrowEffect->GetActorQuat().Equals(DesiredQuat, 0.01f) ||
            !SpawnedArrowEffect->GetActorLocation().Equals(DesiredLoc, 1.f))
        {
            SpawnedArrowEffect->SetActorLocationAndRotation(DesiredLoc, DesiredQuat);
        }
    }
}

void ATransformation_actor::RefreshOverlappingMetals()
{
    if (!MeshComp) return;

    OverlappingMetals.Empty();

    UWorld* World = GetWorld();
    if (!World) return;

    const FVector Center = GetActorLocation();
    FCollisionObjectQueryParams Obj = FCollisionObjectQueryParams::AllObjects;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(MagnetSense), false);
    Q.AddIgnoredActor(this);

    TArray<FOverlapResult> Hits;
    World->OverlapMultiByObjectType(Hits, Center, FQuat::Identity, Obj,
        FCollisionShape::MakeSphere(MaxDistance), Q);

    for (const FOverlapResult& H : Hits)
    {
        UPrimitiveComponent* Comp = H.GetComponent();
        if (!Comp) continue;

        AActor* CompOwner = Comp->GetOwner();
        if (!CompOwner || CompOwner == this) continue;

        bool bIsMetal = CompOwner->ActorHasTag(MetalTag);
        bool bIsMagnet = CompOwner->ActorHasTag(MagnetTag);

        if (bIsMetal && Comp->IsSimulatingPhysics())
        {
            OverlappingMetals.Add(Comp);
        }
        else if (bIsMagnet)
        {
            OverlappingMetals.Add(Comp);
        }
    }
}