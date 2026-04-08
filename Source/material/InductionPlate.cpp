#include "InductionPlate.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"

AInductionPlate::AInductionPlate()
{
	PrimaryActorTick.bCanEverTick = true;

	PlateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlateMesh"));
	SetRootComponent(PlateMesh);
	PlateMesh->SetMobility(EComponentMobility::Movable);
	PlateMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	PlateMesh->SetGenerateOverlapEvents(true);
}

void AInductionPlate::BeginPlay()
{
	Super::BeginPlay();
	TemperatureC = 20.f;

	
}

void AInductionPlate::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (PlateMesh)
	{
		float TempRatio = FMath::Clamp((TemperatureC - 20.f) / 780.f, 0.f, 1.f);
		int32 StencilVal = FMath::RoundToInt(TempRatio * 255.f);
		PlateMesh->SetRenderCustomDepth(StencilVal > 0);
		PlateMesh->SetCustomDepthStencilValue(StencilVal);
	}

#if ENABLE_DRAW_DEBUG
	if (bDebugDraw)
	{
		DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 40.f),
			FString::Printf(TEXT("Plate: %.0f C"), TemperatureC),
			nullptr, FColor::White, 0.f, true);
	}
#endif
}

void AInductionPlate::ReceiveInductionHeat(float EnergyJ)
{
	if (EnergyJ <= 0.f) return;
	TemperatureC += EnergyJ;
}