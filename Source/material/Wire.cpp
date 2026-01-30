#include "Wire.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SphereComponent.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"

AWire::AWire()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
    Spline->SetupAttachment(Root);

    ConnectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ConnectionSphere"));
    ConnectionSphere->SetupAttachment(Root);
    ConnectionSphere->SetSphereRadius(30.f);
    ConnectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ConnectionSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
}

void AWire::BeginPlay()
{
    Super::BeginPlay();
    UpdateConnectionPoint();
    RefreshConnectedActors();
    ApplyPower();

    if (RefreshInterval > 0.f)
    {
        GetWorldTimerManager().SetTimer(RefreshTimerHandle, this, &AWire::RefreshConnectedActors, RefreshInterval, true);
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

void AWire::UpdateConnectionPoint()
{
    if (Spline && ConnectionSphere)
    {
        FVector Point0Location = Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local);
        ConnectionSphere->SetRelativeLocation(Point0Location);
        ConnectionSphere->SetSphereRadius(OverlapRadius);
    }
}

void AWire::SetPowered(bool bNewPowered)
{
    bPowered = bNewPowered;
    ApplyPower();
    PropagatePowerToConnected();
}

void AWire::ApplyPower()
{
    UMaterialInterface* TargetMat = bPowered ? OnMaterial : OffMaterial;
    if (!TargetMat) return;

    for (USplineMeshComponent* Mesh : SegmentMeshes)
    {
        if (IsValid(Mesh))
        {
            const int32 NumMaterials = Mesh->GetNumMaterials();
            for (int32 i = 0; i < NumMaterials; ++i)
            {
                Mesh->SetMaterial(i, TargetMat);
            }
            Mesh->MarkRenderStateDirty();
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Wire: %s ApplyPower -> bPowered: %s (Mat: %s)"), 
        *GetName(), bPowered ? TEXT("TRUE") : TEXT("FALSE"), *TargetMat->GetName());
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

        const FVector StartPos = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
        const FVector StartTan = Spline->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
        const FVector EndPos   = Spline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::Local);
        const FVector EndTan   = Spline->GetTangentAtSplinePoint(i + 1, ESplineCoordinateSpace::Local);

        SplineMesh->SetStartAndEnd(StartPos, StartTan, EndPos, EndTan, true);
        SplineMesh->SetStartScale(SegmentScale);
        SplineMesh->SetEndScale(SegmentScale);

        SplineMesh->RegisterComponent();
        SegmentMeshes.Add(SplineMesh);
    }
    ApplyPower();
}

void AWire::GatherOverlapsAt(const FVector& WorldPos, TArray<AActor*>& OutActors) const
{
    UWorld* World = GetWorld();
    if (!World) return;

    FCollisionObjectQueryParams ObjParams(FCollisionObjectQueryParams::AllObjects);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WireOverlap), false);
    QueryParams.AddIgnoredActor(this);

    TArray<FOverlapResult> Hits;
    World->OverlapMultiByObjectType(Hits, WorldPos, FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(OverlapRadius), QueryParams);

    for (const FOverlapResult& Hit : Hits)
    {
        AActor* A = Hit.GetActor();
        if (A && (!ConnectableClass || A->IsA(ConnectableClass)))
        {
            OutActors.Add(A);
        }
    }
}

void AWire::RefreshConnectedActors()
{
    ConnectedActors.Empty();
    if (!Spline) return;

    TArray<AActor*> Found;
    const FVector Point0World = Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
    GatherOverlapsAt(Point0World, Found);

    for (AActor* A : Found) ConnectedActors.AddUnique(A);
    PropagatePowerToConnected();
}

void AWire::PropagatePowerToConnected()
{
    for (AActor* Target : ConnectedActors)
    {
        if (UFunction* Func = Target->FindFunction(FName("SetPowered")))
        {
            struct FParams { bool bNewPowered; };
            FParams Params { bPowered };
            Target->ProcessEvent(Func, &Params);
        }
    }
}