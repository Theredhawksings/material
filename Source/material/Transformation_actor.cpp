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
#include "DrawDebugHelpers.h"

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
    World->OverlapMultiByObjectType(Hits, Center, FQuat::Identity, Obj,
        FCollisionShape::MakeSphere(Radius), Q);

    ConnectedWires.Empty();
    bool bAnyPowerFound = false;

    for (const FOverlapResult& H : Hits)
    {
        AWire* Wire = Cast<AWire>(H.GetActor());
        if (!Wire) continue;

        ConnectedWires.AddUnique(Wire);

        if (Wire->IsPowered())
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
    else if (bElectrified)
    {
        EnergizeWiresIfElectrified();
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

    // 물리 ON + 중력 OFF + 속도 0 = 그 자리 고정, 콜리전은 정상 작동
    MeshComp->SetSimulatePhysics(true);
    MeshComp->SetPhysicsLinearVelocity(FVector::ZeroVector);
    MeshComp->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    RefreshOverlappingMetals();
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
}

void ATransformation_actor::OnMagnetHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    return;
}

void ATransformation_actor::UpdateMagnetism(float DeltaTime)
{
    if (bDemagnetized || !MeshComp) return;
    if (MeshComp->GetCollisionEnabled() == ECollisionEnabled::NoCollision) return;

    TimeSinceLastMagnetRefresh += DeltaTime;
    if (TimeSinceLastMagnetRefresh >= MagnetRefreshInterval)
    {
        TimeSinceLastMagnetRefresh = 0.f;
        RefreshOverlappingMetals();

        UE_LOG(LogTemp, Log, TEXT("[%s] MagStr=%.0f, Targets=%d"),
            *GetName(), MagnetStrength, OverlappingMetals.Num());
    }

    if (OverlappingMetals.Num() == 0) return;

    const FVector MagnetLoc = GetActorLocation();
    const FVector MyNorth = GetNorthPoleWorldDir();
    const float StrengthTimesMultiplier = MagnetStrength * ForceMultiplier;

    for (auto It = OverlappingMetals.CreateIterator(); It; ++It)
    {
        UPrimitiveComponent* Comp = It->Get();
        if (!IsValid(Comp)) { It.RemoveCurrent(); continue; }
        AActor* OwnerActor = Comp->GetOwner();
        if (!OwnerActor) { It.RemoveCurrent(); continue; }
        if (!OwnerActor->ActorHasTag(MetalTag) && !OwnerActor->ActorHasTag(MagnetTag))
        { It.RemoveCurrent(); continue; }
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

        if (OtherMagnetActor)
        {
            if (bMagnetSnapped) continue;

            const FVector OtherNorth = OtherMagnetActor->GetNorthPoleWorldDir();

            const float MyPoleToward = FVector::DotProduct(MyNorth, DirToOther);
            const float OtherPoleToward = FVector::DotProduct(OtherNorth, -DirToOther);
            const float PolarityFactor = -(MyPoleToward * OtherPoleToward);

            float SpeedScale = (ReferenceDistance / FMath::Max(SafeDist, 1.f));
            SpeedScale = FMath::Clamp(SpeedScale, 0.1f, 5.f);

            float Speed = MagnetApproachSpeed * SpeedScale * FMath::Abs(PolarityFactor);

            FVector MoveDir = DirToOther * FMath::Sign(PolarityFactor);

            FVector MoveDelta = MoveDir * Speed * DeltaTime;

            AddActorWorldOffset(MoveDelta, true);

            if (PolarityFactor > 0.f && Distance <= MagnetSnapDistance)
            {
                bMagnetSnapped = true;
                UE_LOG(LogTemp, Log, TEXT("[%s] Snapped to [%s]"), *GetName(), *OtherActor->GetName());
            }

            if (bDebugDraw)
            {
                FColor DebugColor = (PolarityFactor > 0) ? FColor::Green : FColor::Red;
                DrawDebugLine(GetWorld(), MagnetLoc, OtherLoc, DebugColor, false, 0.1f, 0, 2.f);
                DrawDebugString(GetWorld(), MagnetLoc + FVector(0, 0, 50),
                    FString::Printf(TEXT("P=%.2f D=%.0f"), PolarityFactor, Distance),
                    nullptr, FColor::White, 0.1f);
            }
        }
        else
        {
            if (!TargetComp->IsSimulatingPhysics()) continue;

            const FVector DirToMagnet = -DirToOther;
            float ForceMag = StrengthTimesMultiplier / FMath::Pow(SafeDist, MagneticDecayExponent);

            const float MetalMass = TargetComp->GetMass();
            ForceMag *= FMath::Clamp(MetalMass / 5.0f, 0.6f, 2.5f);

            const FVector CurrentVel = TargetComp->GetPhysicsLinearVelocity();
            const FVector DampingForce = -CurrentVel * (VelocityDampingFactor * MetalMass);
            FVector FinalForce = (DirToMagnet * ForceMag) + DampingForce;
            FinalForce = FinalForce.GetClampedToMaxSize(MaxForceClamp);

            TargetComp->AddForce(FinalForce, NAME_None, false);
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