#include "Wire.h"
#include "Transformation_actor.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

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
        const FVector Point0Location = Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local);
        ConnectionSphere->SetRelativeLocation(Point0Location);
        ConnectionSphere->SetSphereRadius(OverlapRadius);
    }
}

void AWire::UpdateFinalPower()
{
    const bool bNewFinal = (bPoweredBySource || bPoweredByMetal);
    if (bPoweredFinal == bNewFinal) return;
    bPoweredFinal = bNewFinal;
    ApplyPower();
    PropagatePowerToConnected();
}

void AWire::SetPowered(bool bNewPowered)
{
    if (bPoweredBySource == bNewPowered) return;
    bPoweredBySource = bNewPowered;
    UpdateFinalPower();
}

void AWire::SetPoweredByMetal(bool bNewPoweredByMetal)
{
    if (bPoweredByMetal == bNewPoweredByMetal) return;
    bPoweredByMetal = bNewPoweredByMetal;
    UpdateFinalPower();
}

void AWire::ApplyPower()
{
    UMaterialInterface* TargetMat = bPoweredFinal ? OnMaterial : OffMaterial;
    if (!TargetMat) return;
    for (USplineMeshComponent* Mesh : SegmentMeshes)
    {
        if (IsValid(Mesh))
        {
            const int32 NumMaterials = Mesh->GetNumMaterials();
            for (int32 i = 0; i < NumMaterials; ++i) Mesh->SetMaterial(i, TargetMat);
            Mesh->MarkRenderStateDirty();
        }
    }
}

void AWire::ClearGeneratedMeshes()
{
    for (USplineMeshComponent* Comp : SegmentMeshes)
    {
        if (Comp) { Comp->UnregisterComponent(); Comp->DestroyComponent(); }
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
        SplineMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        SplineMesh->SetCollisionObjectType(ECC_WorldDynamic);
        SplineMesh->SetGenerateOverlapEvents(true);
        SplineMesh->SetCollisionResponseToAllChannels(ECR_Overlap);
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
    bool bFoundPowerFromMetal = false;

    // 1. 모든 세그먼트(SplineMesh)에서 주변 철 블록 감지
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
                ConnectedActors.AddUnique(A);
                if (ATransformation_actor* Metal = Cast<ATransformation_actor>(A))
                {
                    // 주변 철이 켜져있으면 나(전선)도 켜짐
                    if (Metal->IsElectrified()) bFoundPowerFromMetal = true;
                }
            }
        }
    }

    // 2. [추가] 전선의 시작점 구체(ConnectionSphere)에서도 한번 더 감지 (ㄱ자 꺾임 대비)
    if (!bFoundPowerFromMetal && ConnectionSphere)
    {
        TArray<AActor*> SphereOverlaps;
        ConnectionSphere->GetOverlappingActors(SphereOverlaps);
        for (AActor* A : SphereOverlaps)
        {
            if (ATransformation_actor* Metal = Cast<ATransformation_actor>(A))
            {
                if (Metal->IsElectrified()) 
                {
                    ConnectedActors.AddUnique(A);
                    bFoundPowerFromMetal = true;
                }
            }
        }
    }

    SetPoweredByMetal(bFoundPowerFromMetal);
    PropagatePowerToConnected();
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