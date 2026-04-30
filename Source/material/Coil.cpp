#include "Coil.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Wire.h"
#include "InductionPlate.h"

#include "Engine/OverlapResult.h"

ACoil::ACoil()
{
	PrimaryActorTick.bCanEverTick = true;

	CoilMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoilMesh"));
	SetRootComponent(CoilMesh);
	CoilMesh->SetMobility(EComponentMobility::Movable);
	CoilMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	CoilMesh->SetGenerateOverlapEvents(true);

	DetectionZone = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionZone"));
	DetectionZone->SetupAttachment(RootComponent);
	DetectionZone->SetBoxExtent(DetectionBoxExtent);
	DetectionZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectionZone->SetCollisionResponseToAllChannels(ECR_Overlap);
	DetectionZone->SetGenerateOverlapEvents(true);
	DetectionZone->bDrawOnlyIfSelected = !bShowDebugShapes;
	DetectionZone->ShapeColor = FColor::Cyan;
	DetectionZone->SetHiddenInGame(!bShowDebugShapes);
	DetectionZone->SetVisibility(bShowDebugShapes);

	MagneticFieldSphere = CreateDefaultSubobject<USphereComponent>(TEXT("MagneticFieldSphere"));
	MagneticFieldSphere->SetupAttachment(RootComponent);
	MagneticFieldSphere->SetSphereRadius(MagneticFieldRadius);
	MagneticFieldSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MagneticFieldSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
	MagneticFieldSphere->SetGenerateOverlapEvents(true);
	MagneticFieldSphere->bDrawOnlyIfSelected = !bShowDebugShapes;
	MagneticFieldSphere->ShapeColor = FColor::Blue;
	MagneticFieldSphere->SetHiddenInGame(!bShowDebugShapes);
	MagneticFieldSphere->SetVisibility(bShowDebugShapes);

	BottomBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("BottomBlocker"));
	BottomBlocker->SetupAttachment(RootComponent);
	BottomBlocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BottomBlocker->SetCollisionResponseToAllChannels(ECR_Block);
	BottomBlocker->SetVisibility(false);  // 와이어프레임 숨김
	BottomBlocker->SetHiddenInGame(true);  // 인게임에서도 와이어프레임 숨김
}

void ACoil::BeginPlay()
{
	Super::BeginPlay();

	DetectionZone->SetBoxExtent(DetectionBoxExtent);
	MagneticFieldSphere->SetSphereRadius(MagneticFieldRadius);
	BaseCoilLocation = GetActorLocation();
	CoilMesh->SetSimulatePhysics(false);

	ApplyDebugVisibility();

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		EnableInput(PC);
		if (InputComponent)
		{
			InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ACoil::ToggleCoil);
		}
	}
}

void ACoil::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bCoilActive)
	{
		for (const TWeakObjectPtr<AActor>& MagnetPtr : DetectedMagnets)
		{
			if (AActor* Magnet = MagnetPtr.Get())
			{
				if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Magnet->GetRootComponent()))
				{
					if (Root->IsSimulatingPhysics())
					{
						Root->SetPhysicsLinearVelocity(FVector::ZeroVector);
						Root->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
					}
				}
			}
		}

		DetectedMagnets.Empty();
		CurrentEMF = 0.f;
		OscillationTime = 0.f;
		SetActorLocation(BaseCoilLocation, false);
		DebugVisualize();
		return;
	}

	DetectMagnets();
	ApplyOscillation(DeltaTime);
	UpdateFieldRadius();
	ApplyMagneticForce();
	DebugVisualize();
	UpdateCircuit();
	ApplyInductionHeating(DeltaTime);
}

void ACoil::ToggleCoil()
{
	bCoilActive = !bCoilActive;
	UE_LOG(LogTemp, Log, TEXT("Coil [%s] -> %s"), *GetName(), bCoilActive ? TEXT("ON") : TEXT("OFF"));
}

void ACoil::DetectMagnets()
{
	DetectedMagnets.Empty();
	if (!DetectionZone) return;

	TArray<FOverlapResult> Hits;
	FCollisionQueryParams QParams(SCENE_QUERY_STAT(CoilDetect), false);
	QParams.AddIgnoredActor(this);

	GetWorld()->OverlapMultiByObjectType(
		Hits,
		BaseCoilLocation,
		DetectionZone->GetComponentQuat(),
		FCollisionObjectQueryParams::AllObjects,
		FCollisionShape::MakeBox(DetectionBoxExtent),
		QParams
	);

	for (const FOverlapResult& H : Hits)
	{
		AActor* HitActor = H.GetActor();
		if (!HitActor || HitActor == this) continue;
		if (!HitActor->ActorHasTag(MagnetTag)) continue;

		bool bAlreadyAdded = false;
		for (const TWeakObjectPtr<AActor>& Existing : DetectedMagnets)
		{
			if (Existing.Get() == HitActor) { bAlreadyAdded = true; break; }
		}
		if (!bAlreadyAdded)
		{
			DetectedMagnets.Add(HitActor);
		}
	}

	CurrentEMF = 0.f;
	if (bCoilActive && HasMagnetInside())
	{
		const int32 Count = DetectedMagnets.Num();
		const float ScaledSpeed = OscillationSpeed + (Count - 1) * SpeedPerExtraMagnet;
		float TotalB = MagnetFieldStrengthTesla * Count;
		float VelocityFactor = FMath::Abs(FMath::Cos(OscillationTime * ScaledSpeed));

		float RadiusMeters = (CoilInnerDiameterCM / 100.f) / 2.f;
		float Area = PI * RadiusMeters * RadiusMeters;

		CurrentEMF = CoilWindings * TotalB * Area * ScaledSpeed * VelocityFactor;
	}
}

void ACoil::ApplyOscillation(float DeltaTime)
{
	if (HasMagnetInside())
	{
		OscillationTime += DeltaTime;

		const int32 Count = DetectedMagnets.Num();
		const float ScaledAmplitude = OscillationAmplitude * Count;
		const float ScaledSpeed = OscillationSpeed + (Count - 1) * SpeedPerExtraMagnet;

		const float OffsetZ = FMath::Sin(OscillationTime * ScaledSpeed) * ScaledAmplitude;
		const FVector NewLoc = BaseCoilLocation + FVector(0.f, 0.f, OffsetZ);
		SetActorLocation(NewLoc, false);

		MagneticFieldSphere->SetWorldLocation(BaseCoilLocation);
	}
	else
	{
		OscillationTime = 0.f;
		SetActorLocation(BaseCoilLocation, false);
	}
}

void ACoil::UpdateFieldRadius()
{
	const float DynamicRadius = HasMagnetInside()
		? MagneticFieldRadius + (DetectedMagnets.Num() - 1) * FieldRadiusPerMagnet
		: MagneticFieldRadius;

	MagneticFieldSphere->SetSphereRadius(DynamicRadius);
}

void ACoil::ApplyMagneticForce()
{
	if (!HasMagnetInside()) return;

	for (const TWeakObjectPtr<AActor>& MagnetPtr : DetectedMagnets)
	{
		AActor* Magnet = MagnetPtr.Get();
		if (!Magnet) continue;

		UPrimitiveComponent* MagnetRoot = Cast<UPrimitiveComponent>(Magnet->GetRootComponent());
		if (!MagnetRoot || !MagnetRoot->IsSimulatingPhysics()) continue;

		const FVector Direction = BaseCoilLocation - Magnet->GetActorLocation();
		const float Distance = Direction.Size();
		if (Distance < 1.f) continue;

		const float ForceMag = MagneticForceStrength / (DetectedMagnets.Num() * FMath::Max(Distance * Distance, 100.f));
		const FVector Force = Direction.GetSafeNormal() * ForceMag;

		MagnetRoot->AddForce(Force, NAME_None, true);
	}
}

void ACoil::DebugVisualize()
{
#if ENABLE_DRAW_DEBUG
	if (!bDebugDraw) return;

	const bool bActive = bCoilActive && HasMagnetInside();

	if (!bCoilActive)
	{
		DrawDebugBox(GetWorld(), BaseCoilLocation,
			DetectionBoxExtent, DetectionZone->GetComponentQuat(),
			FColor(40, 40, 40), false, 0.f, 0, 2.f);

		DrawDebugString(GetWorld(), BaseCoilLocation + FVector(0.f, 0.f, 60.f),
			TEXT("Coil OFF (Press F)"), nullptr, FColor::Silver, 0.f, true);
		return;
	}

	const FColor BoxColor = bActive ? FColor::Green : FColor::Red;
	DrawDebugBox(GetWorld(), BaseCoilLocation,
		DetectionBoxExtent, DetectionZone->GetComponentQuat(),
		BoxColor, false, 0.f, 0, 2.f);

	const float CurrentRadius = MagneticFieldSphere->GetScaledSphereRadius();
	const FColor SphereColor = bActive ? FColor::Blue : FColor(80, 80, 80);
	DrawDebugSphere(GetWorld(), BaseCoilLocation,
		CurrentRadius, 24, SphereColor, false, 0.f, 0, 1.f);

	if (bActive)
	{
		DrawDebugString(GetWorld(), BaseCoilLocation + FVector(0.f, 0.f, 60.f),
			FString::Printf(TEXT("Magnets: %d | EMF: %.2f V | N: %d"),
				DetectedMagnets.Num(), CurrentEMF, CoilWindings),
			nullptr, FColor::Green, 0.f, true);
	}

	for (const TWeakObjectPtr<AActor>& MagnetPtr : DetectedMagnets)
	{
		if (AActor* M = MagnetPtr.Get())
		{
			DrawDebugDirectionalArrow(GetWorld(),
				M->GetActorLocation(), BaseCoilLocation,
				20.f, FColor::Yellow, false, 0.f, 0, 2.f);
		}
	}
#endif
}

void ACoil::ApplyDebugVisibility()
{
	// DetectionZone 가시성
	if (DetectionZone)
	{
		DetectionZone->SetHiddenInGame(!bShowDebugShapes);
		DetectionZone->SetVisibility(bShowDebugShapes);
		DetectionZone->bDrawOnlyIfSelected = !bShowDebugShapes;
		DetectionZone->MarkRenderStateDirty();
	}

	// MagneticFieldSphere 가시성
	if (MagneticFieldSphere)
	{
		MagneticFieldSphere->SetHiddenInGame(!bShowDebugShapes);
		MagneticFieldSphere->SetVisibility(bShowDebugShapes);
		MagneticFieldSphere->bDrawOnlyIfSelected = !bShowDebugShapes;
		MagneticFieldSphere->MarkRenderStateDirty();
	}

	// BottomBlocker는 항상 보이므로 여기서 제어하지 않음
}

#if WITH_EDITOR
void ACoil::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName = (PropertyChangedEvent.Property != nullptr)
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (PropName == GET_MEMBER_NAME_CHECKED(ACoil, bShowDebugShapes))
	{
		ApplyDebugVisibility();
	}
}
#endif

void ACoil::UpdateCircuit()
{
    for (const TWeakObjectPtr<AActor>& WirePtr : ConnectedWires)
    {
        if (AWire* W = Cast<AWire>(WirePtr.Get()))
            W->SetPowered(false);
    }
    ConnectedWires.Empty();

    if (!bCoilActive || CurrentEMF <= 0.f) return;

    FCollisionQueryParams QParams(SCENE_QUERY_STAT(CoilCircuit), false);
    QParams.AddIgnoredActor(this);

    TArray<FOverlapResult> Hits;
    GetWorld()->OverlapMultiByObjectType(
        Hits, BaseCoilLocation, FQuat::Identity,
        FCollisionObjectQueryParams::AllObjects,
        FCollisionShape::MakeSphere(WireDetectRadius), QParams);

    for (const FOverlapResult& H : Hits)
    {
        AWire* Wire = Cast<AWire>(H.GetActor());
        if (!Wire) continue;

        Wire->SetPowered(true);
        Wire->SetBatteryVoltage(CurrentEMF);
        ConnectedWires.Add(Wire);
    }
}

void ACoil::ApplyInductionHeating(float DeltaTime)
{
	if (!bCoilActive || CurrentEMF <= 0.f) return;

	FCollisionQueryParams QParams(SCENE_QUERY_STAT(CoilInduction), false);
	QParams.AddIgnoredActor(this);

	TArray<FOverlapResult> Hits;
	float Radius = MagneticFieldSphere->GetScaledSphereRadius();

	GetWorld()->OverlapMultiByObjectType(
		Hits, BaseCoilLocation, FQuat::Identity,
		FCollisionObjectQueryParams::AllObjects,
		FCollisionShape::MakeSphere(Radius), QParams);

	for (const FOverlapResult& H : Hits)
	{
		AInductionPlate* Plate = Cast<AInductionPlate>(H.GetActor());
		if (!Plate) continue;

		float Distance = FVector::Dist(BaseCoilLocation, Plate->GetActorLocation());
		float DistFactor = 1.f - FMath::Clamp(Distance / Radius, 0.f, 1.f);
		float Energy = CurrentEMF * InductionHeatingRate * DistFactor * DeltaTime;

		Plate->ReceiveInductionHeat(Energy);
	}
}