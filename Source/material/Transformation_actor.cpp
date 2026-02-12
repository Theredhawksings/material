#include "Transformation_actor.h"

#include "Components/StaticMeshComponent.h"

#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include "UObject/ConstructorHelpers.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"

#include "Temperature.h"
#include "Wire.h"

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

    // === ICE 녹는 로직 ===
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
    
    // === WOOD 연소 로직 ===
    if (CurrentForm == EBlockForm::Wood)
    {
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Cyan, 
            FString::Printf(TEXT("Wood State - Burning: %s, Temp: %.1f°C, Target: %.0f°C"), 
            bIsBurning ? TEXT("YES") : TEXT("NO"), WoodTemperatureC, WoodIgnitionTempC));
    }

    if (!bIsBurning)
    {
        if (bHeating && CurrentFire && MeshComp)
        {
            const float DistCm = FVector::Dist(CurrentFire->GetActorLocation(), GetActorLocation());
            
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(2, 0.0f, FColor::Yellow, 
                    FString::Printf(TEXT("Heating! Distance: %.1fcm, MaxDist: %.1fcm"), 
                    DistCm, CurrentFire->MaxHeatDistance));
            }

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

                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(3, 0.0f, FColor::Orange, 
                        FString::Printf(TEXT("Power: %.1fW, DeltaT: %.3f°C, TimeScale: %.0fx"), 
                        ReceivedPowerW, DeltaT, WoodSimTimeScale));
                }

                if (WoodTemperatureC >= WoodIgnitionTempC)
                {
                    bIsBurning = true;
                    CurrentWoodMassKg = WoodMassKg;

                    if (GEngine)
                    {
                        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, 
                            FString::Printf(TEXT("🔥🔥🔥 WOOD IGNITED at %.0f°C! 🔥🔥🔥"), WoodTemperatureC));
                    }
                }
            }
        }
        else
        {
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(4, 0.0f, FColor::Red, 
                    FString::Printf(TEXT("NOT Heating! bHeating:%s, Fire:%s, Mesh:%s"), 
                    bHeating ? TEXT("Y") : TEXT("N"),
                    CurrentFire ? TEXT("Y") : TEXT("N"),
                    MeshComp ? TEXT("Y") : TEXT("N")));
            }
        }
    }
    else
    {
        const float BurnedMassKg = BurnRateKgPerSec * DeltaTime;
        CurrentWoodMassKg -= BurnedMassKg;
        CurrentWoodMassKg = FMath::Max(CurrentWoodMassKg, 0.0f);

        BurnAlpha = 1.0f - (CurrentWoodMassKg / FMath::Max(WoodMassKg, 0.01f));
        
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(5, 0.0f, FColor::Red, 
                FString::Printf(TEXT("🔥 BURNING! Alpha: %.2f, Mass: %.2f/%.2f kg"), 
                BurnAlpha, CurrentWoodMassKg, WoodMassKg));
        }
        
        ApplyWoodBurnVisual(BurnAlpha);

        if (CurrentWoodMassKg <= 0.0f)
        {
            if (bDestroyWhenBurned)
            {
                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, 
                        TEXT("💀 Wood Completely Burned!"));
                }
                Destroy();
            }
            else
            {
                bIsBurning = false;
            }
        }
    }
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
    FVector SavedBaseScale = BaseScaleBeforeMelt;

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

    CurrentForm = NewForm;

    if (bAutoUpdateTags)
    {
        UpdateTagsForForm(NewForm);
    }

    if (const FBlockFormSpec* Spec = FindSpec(CurrentForm))
    {
        ApplySpec(*Spec);
    }

    if (MeshComp && SavedMeltAlpha > 0.0f)
    {
        MeshComp->SetWorldScale3D(SavedCurrentScale);
    }

    if (CurrentForm == EBlockForm::Ice)
    {
        BaseScaleBeforeMelt = SavedBaseScale;
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
}

void ATransformation_actor::NextForm()
{
    if (CycleOrder.Num() <= 0) return;
    int32 Idx = CycleOrder.Find(CurrentForm);
    if (Idx == INDEX_NONE)
    {
        SetForm(CycleOrder[0]);
        return;
    }
    Idx = (Idx + 1) % CycleOrder.Num();
    SetForm(CycleOrder[Idx]);
}

void ATransformation_actor::UpdateTagsForForm(EBlockForm Form)
{
    ClearAllFormTags();
    switch (Form)
    {
    case EBlockForm::Ice: Tags.AddUnique(IceTag); break;
    case EBlockForm::Metal: Tags.AddUnique(MetalTag); break;
    case EBlockForm::Wood: Tags.AddUnique(WoodTag); break;
    case EBlockForm::Rubber: Tags.AddUnique(RubberTag); break;
    default: break;
    }
}

void ATransformation_actor::ClearAllFormTags()
{
    Tags.Remove(IceTag);
    Tags.Remove(MetalTag);
    Tags.Remove(WoodTag);
    Tags.Remove(RubberTag);
}

void ATransformation_actor::StartHeating(ATemperature* FireRef)
{
    CurrentFire = FireRef;
    bHeating = (CurrentFire != nullptr);
    
    // Ice와 Wood만 가열 가능
    if (CurrentForm != EBlockForm::Ice && CurrentForm != EBlockForm::Wood)
    {
        bHeating = false;
    }
}

void ATransformation_actor::StopHeating()
{
    bHeating = false;
    CurrentFire = nullptr;
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

// ==================== ICE ====================

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

// ==================== WOOD ====================

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

    // ρ = m/V → V_new = m_new/ρ → Scale ∝ ∛V
    const float MassRatio = (1.0f - A);  
    const float VolumeRatio = FMath::Max(MassRatio, MinBurnScaleRatio);
    const float ScaleRatio = FMath::Pow(VolumeRatio, 1.0f / 3.0f);  // 입방근
    
    const FVector NewScale = BaseScaleBeforeBurn * ScaleRatio;
    MeshComp->SetWorldScale3D(NewScale);
}