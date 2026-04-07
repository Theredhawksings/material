#include "Coil.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
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
	DetectionZone->SetHiddenInGame(false);
	DetectionZone->ShapeColor = FColor::Cyan;

	MagneticFieldSphere = CreateDefaultSubobject<USphereComponent>(TEXT("MagneticFieldSphere"));
	MagneticFieldSphere->SetupAttachment(RootComponent);
	MagneticFieldSphere->SetSphereRadius(MagneticFieldRadius);
	MagneticFieldSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MagneticFieldSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
	MagneticFieldSphere->SetGenerateOverlapEvents(true);
	MagneticFieldSphere->SetHiddenInGame(false);
	MagneticFieldSphere->ShapeColor = FColor::Blue;

	BottomBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("BottomBlocker"));
	BottomBlocker->SetupAttachment(RootComponent);
	BottomBlocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BottomBlocker->SetCollisionResponseToAllChannels(ECR_Block);
	BottomBlocker->SetHiddenInGame(false);
	BottomBlocker->ShapeColor = FColor::Red;
}

void ACoil::BeginPlay()
{
	Super::BeginPlay();

	DetectionZone->SetBoxExtent(DetectionBoxExtent);
	MagneticFieldSphere->SetSphereRadius(MagneticFieldRadius);
	BaseCoilLocation = GetActorLocation();
	CoilMesh->SetSimulatePhysics(false);

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

		const float ForceMag = MagneticForceStrength / FMath::Max(Distance * Distance, 100.f);
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
			FString::Printf(TEXT("Magnets: %d | Radius: %.0f | Field ON"),
				DetectedMagnets.Num(), CurrentRadius),
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