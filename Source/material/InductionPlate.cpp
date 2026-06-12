#include "InductionPlate.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Transformation_actor.h"
#include "Engine/World.h"
#include "Wire.h"
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

	// ────────────────────────────────────────────
	//  열화상 스텐실 (온도 → 0~255)
	// ────────────────────────────────────────────
	if (PlateMesh)
	{
		float TempRatio = FMath::Clamp((TemperatureC - 20.f) / 780.f, 0.f, 1.f);
		int32 StencilVal = FMath::RoundToInt(TempRatio * 255.f);
		PlateMesh->SetRenderCustomDepth(StencilVal > 0);
		PlateMesh->SetCustomDepthStencilValue(StencilVal);
	}

	// ────────────────────────────────────────────
	//  ★ 연결된 전선에서 전력 받아 가열 (P = V * I)
	//  전선 시작점/끝점이 WireConnectRadius 안에 있어야 연결로 인정
	// ────────────────────────────────────────────
	{
		TArray<FOverlapResult> Hits;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(PlateWireSense), false);
		Q.AddIgnoredActor(this);

		GetWorld()->OverlapMultiByObjectType(
			Hits,
			GetActorLocation(),
			FQuat::Identity,
			FCollisionObjectQueryParams::AllObjects,
			FCollisionShape::MakeSphere(WireConnectRadius * 2.f),  // 넉넉히 스캔
			Q);

		for (const FOverlapResult& H : Hits)
		{
			AWire* Wire = Cast<AWire>(H.GetActor());
			if (!Wire) continue;
			if (!Wire->IsPowered() || Wire->GetEffectiveVoltage() <= 0.f) continue;

			// 전선의 시작점 또는 끝점이 플레이트 근처에 있어야 "연결"로 인정
			const float DistStart = FVector::Dist(Wire->GetStartPointLocation(), GetActorLocation());
			const float DistEnd   = FVector::Dist(Wire->GetEndPointLocation(),   GetActorLocation());
			if (FMath::Min(DistStart, DistEnd) > WireConnectRadius) continue;

			// 전력 → 가열
			const float PowerW = Wire->GetEffectiveVoltage() * Wire->GetEffectiveCurrent();
			ReceiveInductionHeat(PowerW * WireHeatingRate * DeltaTime);
		}
	}

	// ────────────────────────────────────────────
	//  뜨거우면 위에 있는 Metal / Magnet 블록에 열 전달
	// ────────────────────────────────────────────
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

			// ★ Metal과 Magnet 둘 다 가열 대상 (자석 올리면 자성 소실로 이어짐)
			if (!Block->ActorHasTag(TEXT("Metal")) && !Block->ActorHasTag(TEXT("Magnet")))
				continue;

			const float Energy = (TemperatureC - 20.f) * HeatTransferRate * DeltaTime;
			Block->AddFormHeat(Energy);
		}
	}

	// ────────────────────────────────────────────
	//  자연 냉각
	// ────────────────────────────────────────────
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