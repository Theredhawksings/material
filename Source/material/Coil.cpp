#include "Coil.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"

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

	DetectionZone->OnComponentBeginOverlap.AddDynamic(this, &ACoil::OnDetectionBeginOverlap);
	DetectionZone->OnComponentEndOverlap.AddDynamic(this, &ACoil::OnDetectionEndOverlap);
}

void ACoil::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CleanupDeadReferences();

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

void ACoil::OnDetectionBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this) return;
	if (!OtherActor->ActorHasTag(MagnetTag)) return;

	for (const TWeakObjectPtr<AActor>& Existing : DetectedMagnets)
	{
		if (Existing.Get() == OtherActor) return;
	}

	DetectedMagnets.Add(OtherActor);

	UE_LOG(LogTemp, Log, TEXT("[Coil %s] Magnet IN: %s (Total: %d)"),
		*GetName(), *OtherActor->GetName(), DetectedMagnets.Num());
}

void ACoil::OnDetectionEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor) return;

	for (int32 i = DetectedMagnets.Num() - 1; i >= 0; --i)
	{
		if (DetectedMagnets[i].Get() == OtherActor)
		{
			DetectedMagnets.RemoveAt(i);
			break;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Coil %s] Magnet OUT: %s (Remaining: %d)"),
		*GetName(), *OtherActor->GetName(), DetectedMagnets.Num());
}

void ACoil::CleanupDeadReferences()
{
	for (int32 i = DetectedMagnets.Num() - 1; i >= 0; --i)
	{
		if (!DetectedMagnets[i].IsValid())
		{
			DetectedMagnets.RemoveAt(i);
		}
	}
}