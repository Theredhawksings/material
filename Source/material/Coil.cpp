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
}

void ACoil::BeginPlay()
{
    Super::BeginPlay();

    DetectionZone->SetBoxExtent(DetectionBoxExtent);
    MagneticFieldSphere->SetSphereRadius(MagneticFieldRadius);
    BaseCoilLocation = GetActorLocation();

    // ★ 코일 물리 끄기 (수동으로 위치 제어하니까)
    CoilMesh->SetSimulatePhysics(false);
}

void ACoil::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ── 자석 감지 ──
	DetectedMagnets.Empty();

	if (DetectionZone)
	{
		TArray<FOverlapResult> Hits;
		FCollisionQueryParams QParams(SCENE_QUERY_STAT(CoilDetect), false);
		QParams.AddIgnoredActor(this);

		GetWorld()->OverlapMultiByObjectType(
			Hits,
			DetectionZone->GetComponentLocation(),
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

	// ── 코일(스프링) 위아래 진동 ──
// ── 코일(스프링) 위아래 진동 ──
if (HasMagnetInside())
{
    OscillationTime += DeltaTime;

    float OffsetZ = FMath::Sin(OscillationTime * OscillationSpeed) * OscillationAmplitude;
    FVector NewLoc = BaseCoilLocation + FVector(0.f, 0.f, OffsetZ);
    SetActorLocation(NewLoc, false);  // ★ sweep = false

    MagneticFieldSphere->SetWorldLocation(BaseCoilLocation);
}
else
{
    OscillationTime = 0.f;
    SetActorLocation(BaseCoilLocation, false);  // ★ sweep = false
}

	// ── 디버그 ──
#if ENABLE_DRAW_DEBUG
	if (bDebugDraw)
	{
		FColor BoxColor = HasMagnetInside() ? FColor::Green : FColor::Red;
		DrawDebugBox(GetWorld(), DetectionZone->GetComponentLocation(),
			DetectionBoxExtent, DetectionZone->GetComponentQuat(),
			BoxColor, false, 0.f, 0, 2.f);

		FColor SphereColor = HasMagnetInside() ? FColor::Blue : FColor(80, 80, 80);
		DrawDebugSphere(GetWorld(), MagneticFieldSphere->GetComponentLocation(),
			MagneticFieldRadius, 24, SphereColor, false, 0.f, 0, 1.f);

		if (HasMagnetInside())
		{
			DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 60.f),
				FString::Printf(TEXT("Magnet: %d | Field ON"), DetectedMagnets.Num()),
				nullptr, FColor::Green, 0.f, true);
		}
	}
#endif
}