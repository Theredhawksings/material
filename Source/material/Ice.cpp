// Ice.cpp

#include "Ice.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "Temperature.h"

AIce::AIce()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	SetRootComponent(MeshComp);

	MeshComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	MeshComp->SetGenerateOverlapEvents(true);
}

void AIce::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (MeshComp)
	{
		InitialScale = MeshComp->GetComponentScale();
		RecalcMassAndEnergy();
		ApplyMeltVisual(MeltAlpha);
	}
}

void AIce::BeginPlay()
{
	Super::BeginPlay();

	if (MeshComp)
	{
		InitialScale = MeshComp->GetComponentScale();
		RecalcMassAndEnergy();

		if (IceMeltMaterial)
		{
			IceMI = UMaterialInstanceDynamic::Create(IceMeltMaterial, this);
			MeshComp->SetMaterial(0, IceMI);
		}

		ApplyMeltVisual(MeltAlpha);
	}
}

void AIce::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bHeating || !CurrentFire || MeltAlpha >= 1.0f || !MeshComp)
	{
		return;
	}

	const float DistCm = FVector::Dist(CurrentFire->GetActorLocation(), GetActorLocation());

	if (CurrentFire->MaxHeatDistance > 0.0f && DistCm > CurrentFire->MaxHeatDistance)
	{
    return;
	}

	const float DistM = FMath::Max(DistCm / 100.0f, 0.05f);

	const float PtotalW = CurrentFire->GetTotalRadiantPowerW();
	float HeatFluxWm2 = PtotalW / (4.0f * PI * DistM * DistM);

	float ReceivedPowerW = HeatFluxWm2 * EffectiveAreaM2;

	if (CurrentFire->MaxHeatDistance > 0.0f)
	{
    const float Fade = FMath::Clamp(1.0f - (DistCm / CurrentFire->MaxHeatDistance), 0.0f, 1.0f);
    ReceivedPowerW *= Fade;
	}

	if (ReceivedPowerW <= 0.0f)
	{
		return;
	}

	EnergyAccumJ += ReceivedPowerW * DeltaTime * FMath::Max(SimTimeScale, 0.0f);
	MeltAlpha = FMath::Clamp(EnergyAccumJ / FMath::Max(TotalMeltEnergyJ, 1.0f), 0.0f, 1.0f);

	ApplyMeltVisual(MeltAlpha);

	if (bDebugMelt && GEngine)
	{
		DebugAcc += DeltaTime;
		if (DebugAcc >= 0.25f)
		{
			DebugAcc = 0.0f;
			const FVector S = GetActorScale3D();
			const uint64 Key = (uint64)GetUniqueID();

		}
	}

	if (MeltAlpha >= 1.0f)
	{
		if (bDestroyMeshWhenMelted)
		{
			MeshComp->DestroyComponent();
			MeshComp = nullptr;
		}
	}
}

void AIce::StartHeating(ATemperature* FireRef)
{
	CurrentFire = FireRef;
	bHeating = (CurrentFire != nullptr);

	if (bDebugMelt && GEngine)
	{
    const uint64 Key = (uint64)GetUniqueID();
	GEngine->AddOnScreenDebugMessage(Key + 1ULL, 1.0f, FColor::Green, TEXT("StartHeating OK"));
	}

}

void AIce::StopHeating()
{
	bHeating = false;
	CurrentFire = nullptr;
}

void AIce::RecalcMassAndEnergy()
{
	if (!MeshComp)
	{
		VolumeM3 = 1.0f;
		EffectiveAreaM2 = 1.0f;
		TotalMeltEnergyJ = IceDensityKgM3 * VolumeM3 * LatentHeatJPerKg;
		return;
	}

	const FBoxSphereBounds B = MeshComp->Bounds;
	const FVector SizeCm = B.BoxExtent * 2.0f;
	const FVector SizeM = SizeCm / 100.0f;

	VolumeM3 = FMath::Max(SizeM.X * SizeM.Y * SizeM.Z, 1e-6f);

	const float Axy = SizeM.X * SizeM.Y;
	const float Axz = SizeM.X * SizeM.Z;
	const float Ayz = SizeM.Y * SizeM.Z;
	EffectiveAreaM2 = FMath::Max3(Axy, Axz, Ayz);

	const float MassKg = IceDensityKgM3 * VolumeM3;
	TotalMeltEnergyJ = FMath::Max(MassKg * LatentHeatJPerKg, 1.0f);
}

void AIce::ApplyMeltVisual(float Alpha01)
{
	if (!MeshComp) return;

	const float A = FMath::Clamp(Alpha01, 0.0f, 1.0f);

	const float Ratio = FMath::Clamp(MinScaleRatio, 0.0f, 1.0f);
	const FVector TargetScale = InitialScale * Ratio;
	const FVector NewScale = FMath::Lerp(InitialScale, TargetScale, A);

	MeshComp->SetWorldScale3D(NewScale);

	if (IceMI)
	{
		IceMI->SetScalarParameterValue(MeltParamName, A);
	}
}
