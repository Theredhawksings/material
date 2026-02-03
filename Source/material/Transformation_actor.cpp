// Transformation_actor.cpp
#include "Transformation_actor.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"

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
    }
}

void ATransformation_actor::SetPowered(bool bNewPowered)
{
    if (CurrentForm != EBlockForm::Metal) return;

    SetElectrified(bNewPowered);
    if (bNewPowered)
    {
        EnergizeWiresIfElectrified();
    }
}

void ATransformation_actor::RefreshConnectedWires()
{
    if (CurrentForm != EBlockForm::Metal || !MeshComp)
    {
        ConnectedWires.Empty();
        SetElectrified(false);
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

    if (bDrawElectricDebug)
    {
        DrawDebugSphere(World, Center, Radius, 18, bElectrified ? FColor::Green : FColor::Yellow, false, 0.21f, 0, 2.0f);
    }

    ConnectedWires.Empty();
    bool bAnyPowered = false;

    for (const FOverlapResult& H : Hits)
    {
        AActor* A = H.GetActor();
        if (!A || A == this) continue;

        AWire* Wire = Cast<AWire>(A);
        if (!Wire) continue;

        ConnectedWires.AddUnique(Wire);
        
        // IsPowered()는 bPoweredFinal을 반환 (Source든 Metal이든 전기 있으면 true)
        if (Wire->IsPowered()) 
        {
            bAnyPowered = true;
        }
    }

    SetElectrified(bAnyPowered);

    if (bElectrified)
    {
        EnergizeWiresIfElectrified();
    }
    else
    {
        for (auto It = WiresEnergizedByMetal.CreateIterator(); It; ++It)
        {
            if (AWire* W = It->Get())
            {
                W->SetPoweredByMetal(false);
            }
        }
        WiresEnergizedByMetal.Empty();
    }
}

void ATransformation_actor::SetElectrified(bool bNewElectrified)
{
    if (bElectrified == bNewElectrified) return;

    bElectrified = bNewElectrified;

    if (bDebugElectric)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Metal] %s Electrified=%s"), *GetName(), bElectrified ? TEXT("ON") : TEXT("OFF"));

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                (uint64)GetUniqueID(),
                1.2f,
                bElectrified ? FColor::Cyan : FColor::Red,
                FString::Printf(TEXT("IRON [%s] ELECTRIFIED: %s"), *GetName(), bElectrified ? TEXT("ON") : TEXT("OFF"))
            );
        }
    }
}

void ATransformation_actor::EnergizeWiresIfElectrified()
{
    TSet<TWeakObjectPtr<AWire>> Current;

    for (AWire* Wire : ConnectedWires)
    {
        if (!Wire) continue;

        Wire->SetPoweredByMetal(true);
        Current.Add(Wire);
    }

    for (auto It = WiresEnergizedByMetal.CreateIterator(); It; ++It)
    {
        if (!Current.Contains(*It))
        {
            if (AWire* W = It->Get())
            {
                W->SetPoweredByMetal(false);
            }
        }
    }

    WiresEnergizedByMetal = MoveTemp(Current);
}

void ATransformation_actor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (CurrentForm != EBlockForm::Ice) return;
    if (!bHeating || !CurrentFire || !MeshComp) return;
    if (MeltAlpha >= 1.0f) return;

    const float DistCm = FVector::Dist(CurrentFire->GetActorLocation(), GetActorLocation());
    if (CurrentFire->MaxHeatDistance > 0.0f && DistCm > CurrentFire->MaxHeatDistance) return;

    const float DistM = FMath::Max(DistCm / 100.0f, 0.05f);
    const float PtotalW = CurrentFire->GetTotalRadiantPowerW();
    float HeatFluxWm2 = PtotalW / (4.0f * PI * DistM * DistM);
    float ReceivedPowerW = HeatFluxWm2 * EffectiveAreaM2;

    if (CurrentFire->MaxHeatDistance > 0.0f)
    {
        const float Fade = FMath::Clamp(1.0f - (DistCm / CurrentFire->MaxHeatDistance), 0.0f, 1.0f);
        ReceivedPowerW *= Fade;
    }

    if (ReceivedPowerW <= 0.0f) return;

    EnergyAccumJ += ReceivedPowerW * DeltaTime * FMath::Max(SimTimeScale, 0.0f);
    MeltAlpha = FMath::Clamp(EnergyAccumJ / FMath::Max(TotalMeltEnergyJ, 1.0f), 0.0f, 1.0f);
    ApplyIceMeltVisual(MeltAlpha);

    if (bDebugMelt && GEngine)
    {
        DebugAcc += DeltaTime;
        if (DebugAcc >= 0.25f)
        {
            DebugAcc = 0.0f;
            const FVector S = MeshComp->GetComponentScale();
            const FString Msg = FString::Printf(
                TEXT("ICE MELT | d=%.0fcm | W=%.1f | J=%.0f | A=%.3f | S=(%.2f,%.2f,%.2f)"),
                DistCm, ReceivedPowerW, EnergyAccumJ, MeltAlpha, S.X, S.Y, S.Z
            );
            GEngine->AddOnScreenDebugMessage((uint64)GetUniqueID(), 0.3f, FColor::Cyan, Msg);
        }
    }

    if (MeltAlpha >= 1.0f && bDestroyWhenMelted)
    {
        Destroy();
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
    if (CurrentForm != EBlockForm::Ice) return;
    CurrentFire = FireRef;
    bHeating = (CurrentFire != nullptr);
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

void ATransformation_actor::EnterIceMode()
{
    if (!MeshComp) return;
    if (MeltAlpha == 0.0f) EnergyAccumJ = 0.0f;
    DebugAcc = 0.0f;
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
    if (IceMID) IceMID->SetScalarParameterValue(MeltParamName, A);
}