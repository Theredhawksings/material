#include "InductionPlate.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Transformation_actor.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"

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

	// 열화상 스텐실
	if (PlateMesh)
	{
		float TempRatio = FMath::Clamp((TemperatureC - 20.f) / 780.f, 0.f, 1.f);
		int32 StencilVal = FMath::RoundToInt(TempRatio * 255.f);
		PlateMesh->SetRenderCustomDepth(StencilVal > 0);
		PlateMesh->SetCustomDepthStencilValue(StencilVal);
	}

	// 뜨거우면 위에 있는 Metal 블록에 열 전달
	if (TemperatureC > 20.f)
	{
		TArray<FOverlapResult> Hits;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(PlateHeat), false);
		Q.AddIgnoredActor(this);

		GetWorld()->OverlapMultiByObjectType(
			Hits,
			GetActorLocation(),
			FQuat::Identity,
			FCollisionObjectQueryParams::AllObjects,
			FCollisionShape::MakeSphere(HeatTransferRadius),
			Q);

		for (const FOverlapResult& H : Hits)
		{
			ATransformation_actor* Block = Cast<ATransformation_actor>(H.GetActor());
			if (!Block) continue;
			if (!Block->ActorHasTag(TEXT("Metal"))) continue;

			const float Energy = (TemperatureC - 20.f) * HeatTransferRate * DeltaTime;
			Block->AddFormHeat(Energy);
		}
	}

	// 자연 냉각
	if (TemperatureC > 20.f)
		TemperatureC = FMath::Max(TemperatureC - 2.f * DeltaTime, 20.f);

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