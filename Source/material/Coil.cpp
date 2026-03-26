#include "Coil.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
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
}

void ACoil::BeginPlay()
{
	Super::BeginPlay();

	DetectionZone->SetBoxExtent(DetectionBoxExtent);
}

void ACoil::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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

#if ENABLE_DRAW_DEBUG
	if (bDebugDraw)
	{
		FColor StatusColor = HasMagnetInside() ? FColor::Green : FColor::Red;
		DrawDebugBox(GetWorld(), DetectionZone->GetComponentLocation(),
			DetectionBoxExtent, DetectionZone->GetComponentQuat(),
			StatusColor, false, 0.f, 0, 2.f);

		if (HasMagnetInside())
		{
			DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 60.f),
				FString::Printf(TEXT("Magnet: %d"), DetectedMagnets.Num()),
				nullptr, FColor::Green, 0.f, true);
		}
	}
#endif
}