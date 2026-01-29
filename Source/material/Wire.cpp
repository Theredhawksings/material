#include "Wire.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "DrawDebugHelpers.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"

AWire::AWire()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	Spline->SetupAttachment(Root);
	
	// 기본 스플라인 포인트 설정
	Spline->ClearSplinePoints();
	Spline->AddSplinePoint(FVector(0, 0, 0), ESplineCoordinateSpace::Local);
	Spline->AddSplinePoint(FVector(100, 0, 0), ESplineCoordinateSpace::Local);

	bPowered = false;
}

void AWire::BeginPlay()
{
	Super::BeginPlay();

	RefreshConnectedActors();
	ApplyPower();

	if (RefreshInterval > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			RefreshTimerHandle,
			this,
			&AWire::RefreshConnectedActors,
			RefreshInterval,
			true
		);
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

	RebuildSplineMeshes();
	ApplyPower();
}

void AWire::SetPowered(bool bNewPowered)
{
	bPowered = bNewPowered;

	ApplyPower();
	PropagatePowerToConnected();
}

void AWire::ApplyPower()
{
	const float Value = bPowered ? 1.0f : 0.0f;

	for (UMaterialInstanceDynamic* MID : MIDArray)
	{
		if (MID)
		{
			MID->SetScalarParameterValue(PowerParamName, Value);
		}
	}
}

void AWire::ClearGeneratedMeshes()
{
	for (USplineMeshComponent* Comp : SegmentMeshes)
	{
		if (IsValid(Comp))
		{
			Comp->DestroyComponent();
		}
	}

	SegmentMeshes.Reset();
	MIDArray.Reset();
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
        const FVector StartPos = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
        const FVector EndPos   = Spline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::World);
        const FVector StartTan = Spline->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
        const FVector EndTan   = Spline->GetTangentAtSplinePoint(i + 1, ESplineCoordinateSpace::World);

        USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(this, USplineMeshComponent::StaticClass(), *FString::Printf(TEXT("SplineMesh_%d"), i));
        if (!SplineMesh)
        {
            continue;
        }

        SplineMesh->CreationMethod = EComponentCreationMethod::UserConstructionScript;
        SplineMesh->SetupAttachment(Root); 
        SplineMesh->RegisterComponent();

        SplineMesh->SetMobility(EComponentMobility::Movable);
        SplineMesh->SetStaticMesh(SegmentMesh);
        SplineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        SplineMesh->SetGenerateOverlapEvents(false);
        SplineMesh->SetForwardAxis(ESplineMeshAxis::Z, false);

        SplineMesh->SetStartAndEnd(StartPos, StartTan, EndPos, EndTan, true);
        
        SplineMesh->SetWorldLocation(FVector::ZeroVector);
        SplineMesh->SetWorldRotation(FRotator::ZeroRotator);

        SplineMesh->SetStartScale(SegmentScale, true);
        SplineMesh->SetEndScale(SegmentScale, true);

        SegmentMeshes.Add(SplineMesh);

        if (SegmentMaterial)
        {
            UMaterialInstanceDynamic* MID = SplineMesh->CreateDynamicMaterialInstance(0, SegmentMaterial);
            if (MID)
            {
                MIDArray.Add(MID);
            }
        }
    }

    ApplyPower();
}

void AWire::GatherOverlapsAt(const FVector& WorldPos, TArray<AActor*>& OutActors) const
{
	UWorld* World = GetWorld();
	if (!World) return;

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WireOverlap), false);
	QueryParams.AddIgnoredActor(this);

	TArray<FOverlapResult> Hits;
	const bool bHit = World->OverlapMultiByObjectType(
		Hits,
		WorldPos,
		FQuat::Identity,
		ObjParams,
		FCollisionShape::MakeSphere(OverlapRadius),
		QueryParams
	);

	if (!bHit) return;

	for (const FOverlapResult& Hit : Hits)
	{
		AActor* A = Hit.GetActor();
		if (!A) continue;

		if (ConnectableClass && !A->IsA(ConnectableClass))
		{
			continue;
		}

		OutActors.Add(A);
	}
}

void AWire::RefreshConnectedActors()
{
	ConnectedActors.Reset();

	if (!Spline) return;

	TArray<AActor*> Found;

	const FVector StartWorld = Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
	GatherOverlapsAt(StartWorld, Found);

	if (bCheckBothEnds)
	{
		const int32 LastIdx = Spline->GetNumberOfSplinePoints() - 1;
		if (LastIdx >= 0)
		{
			const FVector EndWorld = Spline->GetLocationAtSplinePoint(LastIdx, ESplineCoordinateSpace::World);
			GatherOverlapsAt(EndWorld, Found);
		}
	}

	for (AActor* A : Found)
	{
		ConnectedActors.AddUnique(A);
	}

	PropagatePowerToConnected();
}

void AWire::PropagatePowerToConnected()
{
	for (AActor* Target : ConnectedActors)
	{
		if (!Target) continue;

		if (FBoolProperty* BoolProp = FindFProperty<FBoolProperty>(Target->GetClass(), TEXT("Powered")))
		{
			BoolProp->SetPropertyValue_InContainer(Target, bPowered);
			continue;
		}

		if (UFunction* Func = Target->FindFunction(FName("SetPowered")))
		{
			struct FParams { bool bNewPowered; };
			FParams Params;
			Params.bNewPowered = bPowered;
			Target->ProcessEvent(Func, &Params);
			continue;
		}
	}
}