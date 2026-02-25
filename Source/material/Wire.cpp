#include "Wire.h"
#include "Transformation_actor.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Materials/MaterialInterface.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"

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
    ConnectionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWire::BeginPlay()
{
    Super::BeginPlay();

    UpdateConnectionPoint();
    ApplyPower();

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
}

void AWire::SetBatteryVoltage(float NewVoltage)
{
    BatteryVoltage = FMath::Max(NewVoltage, 0.f);
}

void AWire::UpdateFinalPower()
{
    const bool bNewFinal = (bPoweredBySource || bPoweredByMetal);
    if (bPoweredFinal == bNewFinal)
    {
        return;
    }

    bPoweredFinal = bNewFinal;
    ApplyPower();
    PropagatePowerToConnected();

    if (!bPoweredFinal)
    {
        BatteryVoltage = 0.f;
        RefreshConnectedActors();
    }
}

void AWire::SetPowered(bool bNewPowered)
{
    if (bPoweredBySource == bNewPowered)
    {
        return;
    }

    bPoweredBySource = bNewPowered;

    if (!bNewPowered)
    {
        bPoweredByMetal = false;
    }

    UpdateFinalPower();
}

void AWire::SetPoweredByMetal(bool bNewPoweredByMetal)
{
    if (bPoweredByMetal == bNewPoweredByMetal)
    {
        return;
    }

    bPoweredByMetal = bNewPoweredByMetal;
    UpdateFinalPower();
}

void AWire::UpdateJouleHeating(float DeltaTime)
{
    if (bPoweredFinal && BatteryVoltage > 0.f)
    {
        const float R = FMath::Max(Resistance, 0.01f);
        CurrentAmps = BatteryVoltage / R;

        const float JoulePowerW = CurrentAmps * CurrentAmps * R;
        const float EnergyJ = JoulePowerW * DeltaTime * FMath::Max(SimTimeScale, 0.f);

        const float DeltaT = EnergyJ / FMath::Max(WireMassKg * SpecificHeatJPerKgK, 0.01f);
        WireTemperatureC += DeltaT;
    }
    else
    {
        CurrentAmps = 0.f;
    }

    if (WireTemperatureC > AmbientTemperatureC)
    {
        const float TempDiff = WireTemperatureC - AmbientTemperatureC;
        const float CoolAmount = CoolingRateKPerSec * (TempDiff / 100.f) * DeltaTime;
        WireTemperatureC -= CoolAmount;
        WireTemperatureC = FMath::Max(WireTemperatureC, AmbientTemperatureC);
    }

    WireTemperatureC = FMath::Clamp(WireTemperatureC, AmbientTemperatureC, MaxWireTemperatureC);

#if ENABLE_DRAW_DEBUG
    if (bDebugWire && WireTemperatureC > AmbientTemperatureC + 1.f)
    {
        const FColor TempColor = (WireTemperatureC > 400.f) ? FColor::Red
                                : (WireTemperatureC > 200.f) ? FColor::Orange
                                : FColor::Yellow;

        DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 80.f),
            FString::Printf(TEXT("%.0f°C  %.1fA"), WireTemperatureC, CurrentAmps),
            nullptr, TempColor, 0.0f, true);
    }
#endif
}

void AWire::EmitHeatToNearby(float DeltaTime)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const float T_K = WireTemperatureC + 273.15f;
    const float EmitPowerW = WireEmissivity * StefanBoltzmannSigma * WireSurfaceAreaM2
        * FMath::Pow(T_K, 4.f);

    if (EmitPowerW <= 0.f)
    {
        return;
    }

    TArray<FOverlapResult> Hits;
    FCollisionObjectQueryParams ObjParams = FCollisionObjectQueryParams::AllObjects;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WireHeat), false, this);

    World->OverlapMultiByObjectType(Hits, GetActorLocation(), FQuat::Identity,
        ObjParams, FCollisionShape::MakeSphere(HeatEmitRadius), QueryParams);

    static const FName ReceiveHeatName(TEXT("ReceiveHeatEnergy"));

    for (const FOverlapResult& Hit : Hits)
    {
        AActor* Other = Hit.GetActor();
        if (!Other || Other == this)
        {
            continue;
        }

        const float DistCm = FVector::Dist(GetActorLocation(), Other->GetActorLocation());
        if (DistCm > HeatEmitRadius)
        {
            continue;
        }

        // 거리에 따른 열량: 역제곱 법칙 + 페이드
        const float DistM = FMath::Max(DistCm / 100.f, 0.05f);
        const float FluxWm2 = EmitPowerW / (4.f * PI * DistM * DistM);
        const float Fade = FMath::Clamp(1.f - (DistCm / HeatEmitRadius), 0.f, 1.f);
        const float EnergyJ = FluxWm2 * Fade * DeltaTime;

        if (EnergyJ > 0.f)
        {
            if (UFunction* Fn = Other->FindFunction(ReceiveHeatName))
            {
                float Param = EnergyJ;
                Other->ProcessEvent(Fn, &Param);
            }
        }
    }
}

void AWire::UpdateConnectionPoint()
{
    if (Spline && ConnectionSphere)
    {
        const FVector Point0Location = Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local);
        ConnectionSphere->SetRelativeLocation(Point0Location);
        ConnectionSphere->SetSphereRadius(OverlapRadius);
    }
}

void AWire::ApplyPower()
{
    UMaterialInterface* TargetMat = bPoweredFinal ? OnMaterial : OffMaterial;
    if (!TargetMat)
    {
        return;
    }

    for (USplineMeshComponent* Mesh : SegmentMeshes)
    {
        if (IsValid(Mesh))
        {
            const int32 NumMaterials = Mesh->GetNumMaterials();
            for (int32 i = 0; i < NumMaterials; ++i)
            {
                Mesh->SetMaterial(i, TargetMat);
            }
        }
    }
}

void AWire::ClearGeneratedMeshes()
{
    for (USplineMeshComponent* Comp : SegmentMeshes)
    {
        if (Comp)
        {
            Comp->UnregisterComponent();
            Comp->DestroyComponent();
        }
    }
    SegmentMeshes.Empty();
}

void AWire::RebuildSplineMeshes()
{
    ClearGeneratedMeshes();

    if (!Spline || !SegmentMesh)
    {
        return;
    }

    const int32 NumPoints = Spline->GetNumberOfSplinePoints();
    if (NumPoints < 2)
    {
        return;
    }

    for (int32 i = 0; i < NumPoints - 1; ++i)
    {
        USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(this);
        if (!SplineMesh)
        {
            continue;
        }

        SplineMesh->SetMobility(EComponentMobility::Movable);
        SplineMesh->CreationMethod = EComponentCreationMethod::UserConstructionScript;
        SplineMesh->SetupAttachment(Spline);
        SplineMesh->SetStaticMesh(SegmentMesh);
        SplineMesh->SetForwardAxis(ESplineMeshAxis::Z, false);
        SplineMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        SplineMesh->SetGenerateOverlapEvents(true);
        SplineMesh->SetCollisionObjectType(ECC_WorldDynamic);
        SplineMesh->SetCollisionResponseToAllChannels(ECR_Overlap);
        SplineMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        SplineMesh->RegisterComponent();

        SegmentMeshes.Add(SplineMesh);

        const FVector StartPos = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
        const FVector StartTan = Spline->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
        const FVector EndPos = Spline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::Local);
        const FVector EndTan = Spline->GetTangentAtSplinePoint(i + 1, ESplineCoordinateSpace::Local);

        SplineMesh->SetStartAndEnd(StartPos, StartTan, EndPos, EndTan, true);
        SplineMesh->SetStartScale(SegmentScale);
        SplineMesh->SetEndScale(SegmentScale);
    }

    ApplyPower();
}

void AWire::RefreshConnectedActors()
{
    ConnectedActors.Empty();
    bool bFoundPower = false;

    for (USplineMeshComponent* Segment : SegmentMeshes)
    {
        if (!Segment)
        {
            continue;
        }

        TArray<AActor*> OverlappingActors;
        Segment->GetOverlappingActors(OverlappingActors);

        for (AActor* A : OverlappingActors)
        {
            if (!A || A == this)
            {
                continue;
            }

            if (A->ActorHasTag(FName("Metal")))
            {
                if (ATransformation_actor* Metal = Cast<ATransformation_actor>(A))
                {
                    ConnectedActors.AddUnique(Metal);
                    if (Metal->IsElectrified())
                    {
                        bFoundPower = true;
                    }
                }
            }
        }
    }

    SetPoweredByMetal(bFoundPower);
}

void AWire::PropagatePowerToConnected()
{
    for (AActor* Target : ConnectedActors)
    {
        if (ATransformation_actor* Metal = Cast<ATransformation_actor>(Target))
        {
            Metal->SetPowered(bPoweredFinal);
        }
    }
}