// Wire.cpp
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
#include "Engine/Engine.h"
#include "CollisionQueryParams.h"
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

    if (WireTemperatureC > HeatEmitThresholdC)
    {
        EmitHeatToNearby(DeltaTime);
    }

    // 스플라인 중점에 빨간 구 — 조건 없이 무조건 표시
    if (Spline)
    {
        const float HalfDist = Spline->GetSplineLength() * 0.5f;
        const FVector MidWorld = Spline->GetLocationAtDistanceAlongSpline(HalfDist, ESplineCoordinateSpace::World);
        DrawDebugSphere(GetWorld(), MidWorld, IceHeatZoneRadius, 20, FColor::Red, false, -1.f, 0, 2.f);
    }
}

void AWire::SetBatteryVoltage(float NewVoltage)
{
    BatteryVoltage = FMath::Max(NewVoltage, 0.f);
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
        BatteryVoltage = 0.f;
        RefreshConnectedActors();
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

void AWire::UpdateJouleHeating(float DeltaTime)
{
    if (bPoweredFinal)
    {
        const float R = FMath::Max(Resistance, 0.01f);
        const float V = (BatteryVoltage > 0.f) ? BatteryVoltage : DefaultVoltage;
        CurrentAmps = V / R;

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

    UpdateWireVisual();

    if (bDebugWire && WireTemperatureC > AmbientTemperatureC + 1.f)
    {
        const FColor TempColor = (WireTemperatureC > 400.f) ? FColor::Red
                                : (WireTemperatureC > 200.f) ? FColor::Orange
                                : FColor::Yellow;

        DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 80.f),
            FString::Printf(TEXT("%.0f C  %.1fA"), WireTemperatureC, CurrentAmps),
            nullptr, TempColor, 0.0f, true);
    }
}

void AWire::EmitHeatToNearby(float DeltaTime)
{
    if (!GetWorld()) return;

    const float T_K = WireTemperatureC + 273.15f;
    const float EmitPowerW = WireEmissivity * StefanBoltzmannSigma * WireSurfaceAreaM2
        * FMath::Pow(T_K, 4.f);

    if (EmitPowerW <= 0.f) return;

    FCollisionObjectQueryParams ObjParams;
    ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjParams.AddObjectTypesToQuery(ECC_PhysicsBody);

    FCollisionQueryParams QParams(SCENE_QUERY_STAT(WireHeatOverlap), false);
    QParams.AddIgnoredActor(this);

    auto HeatAt = [&](const FVector& Center, float Radius, float Multiplier)
    {
        TArray<FOverlapResult> Hits;
        GetWorld()->OverlapMultiByObjectType(
            Hits,
            Center,
            FQuat::Identity,
            ObjParams,
            FCollisionShape::MakeSphere(Radius),
            QParams
        );

        for (const FOverlapResult& H : Hits)
        {
            ATransformation_actor* Ice = Cast<ATransformation_actor>(H.GetActor());
            if (!Ice) continue;

            const float DistCm = FVector::Dist(Center, Ice->GetActorLocation());
            const float DistM  = FMath::Max(DistCm / 100.f, 0.05f);

            const float FluxWm2 = EmitPowerW / (4.f * PI * DistM * DistM);
            const float Fade = FMath::Clamp(1.f - (DistCm / Radius), 0.f, 1.f);
            const float EnergyJ = FluxWm2 * IceReceiveAreaM2 * Fade * DeltaTime * Multiplier;

            if (EnergyJ > 0.f)
            {
                Ice->ReceiveHeatEnergy(EnergyJ, WireTemperatureC);
            }
        }
    };

    for (USphereComponent* Sphere : HeatSpheres)
    {
        if (!Sphere) continue;
        HeatAt(Sphere->GetComponentLocation(), HeatEmitRadius, SegmentHeatMultiplier);
    }

    if (IceHeatZone && WireTemperatureC >= IceHeatThresholdC)
    {
        HeatAt(IceHeatZone->GetComponentLocation(), IceHeatZoneRadius, IceHeatMultiplier);
    }
}

void AWire::OnIceHeatZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == this) return;
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
            FString::Printf(TEXT("[IceHeatZone] 진입: %s"), *OtherActor->GetName()));
    }
}

void AWire::OnIceHeatZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!OtherActor || OtherActor == this) return;

    static const FName StopHeatingName(TEXT("StopHeating"));
    if (UFunction* Fn = OtherActor->FindFunction(StopHeatingName))
        OtherActor->ProcessEvent(Fn, nullptr);
}

void AWire::EnsureIceHeating()
{
    if (!IceHeatZone) return;
    if (!bPoweredFinal || WireTemperatureC < IceHeatThresholdC) return;

    TArray<AActor*> Actors;
    IceHeatZone->GetOverlappingActors(Actors);

    static const FName StartHeatingName(TEXT("StartHeating"));
    static const FName IsHeatingName(TEXT("IsHeating"));

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
    SegmentMIDs.Empty();

    for (USplineMeshComponent* Mesh : SegmentMeshes)
    {
        if (!IsValid(Mesh)) continue;

        if (bPoweredFinal && OnMaterial)
        {
            UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(OnMaterial, this);
            Mesh->SetMaterial(0, MID);
            SegmentMIDs.Add(MID);
        }
        else if (OffMaterial)
        {
            const int32 NumMaterials = Mesh->GetNumMaterials();
            for (int32 i = 0; i < NumMaterials; ++i)
                Mesh->SetMaterial(i, OffMaterial);
        }
    }
}

void AWire::UpdateWireVisual()
{
    const float Alpha = FMath::Clamp(WireTemperatureC * WireTempVisualScale, 0.f, 1.f);
    for (UMaterialInstanceDynamic* MID : SegmentMIDs)
        if (MID) MID->SetScalarParameterValue(WireHeatParamName, Alpha);
}

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
        SplineMesh->RegisterComponent();

        SegmentMeshes.Add(SplineMesh);

        const FVector StartPos = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
        const FVector StartTan = Spline->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
        const FVector EndPos   = Spline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::Local);
        const FVector EndTan   = Spline->GetTangentAtSplinePoint(i + 1, ESplineCoordinateSpace::Local);

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
        HeatSphere->SetHiddenInGame(true);
        HeatSphere->SetVisibility(false);
        HeatSphere->RegisterComponent();

        HeatSpheres.Add(HeatSphere);
    }

    // 스플라인 중점에 IceHeatZone (충돌용)
    {
        const int32 MidIndex   = (NumPoints - 1) / 2;
        const FVector MidLocal = Spline->GetLocationAtSplinePoint(MidIndex, ESplineCoordinateSpace::Local);

        IceHeatZone = NewObject<USphereComponent>(this);
        IceHeatZone->SetMobility(EComponentMobility::Movable);
        IceHeatZone->CreationMethod = EComponentCreationMethod::UserConstructionScript;
        IceHeatZone->SetupAttachment(Spline);
        IceHeatZone->SetRelativeLocation(MidLocal);
        IceHeatZone->SetSphereRadius(IceHeatZoneRadius);
        IceHeatZone->SetCollisionProfileName(TEXT("Trigger"));
        IceHeatZone->SetGenerateOverlapEvents(true);
        IceHeatZone->SetHiddenInGame(false);
        IceHeatZone->SetVisibility(true);
        IceHeatZone->bDrawOnlyIfSelected = false;
        IceHeatZone->ShapeColor = FColor::Red;
        IceHeatZone->OnComponentBeginOverlap.AddDynamic(this, &AWire::OnIceHeatZoneBeginOverlap);
        IceHeatZone->OnComponentEndOverlap.AddDynamic(this,   &AWire::OnIceHeatZoneEndOverlap);
        IceHeatZone->RegisterComponent();
    }

    ApplyPower();
}

void AWire::RefreshConnectedActors()
{
    ConnectedActors.Empty();
    bool bFoundPower = false;

    for (USplineMeshComponent* Segment : SegmentMeshes)
    {
        if (!Segment) continue;

        TArray<AActor*> OverlappingActors;
        Segment->GetOverlappingActors(OverlappingActors);

        for (AActor* A : OverlappingActors)
        {
            if (!A || A == this) continue;

            if (A->ActorHasTag(FName("Metal")))
            {
                if (ATransformation_actor* Metal = Cast<ATransformation_actor>(A))
                {
                    ConnectedActors.AddUnique(Metal);
                    if (Metal->IsElectrified()) bFoundPower = true;
                }
            }
        }
    }

    SetPoweredByMetal(bFoundPower);
}

void AWire::PropagatePowerToConnected()
{
    for (AActor* Target : ConnectedActors)
        if (ATransformation_actor* Metal = Cast<ATransformation_actor>(Target))
            Metal->SetPowered(bPoweredFinal);
}