// Temperature.cpp

#include "Temperature.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ShapeComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

ATemperature::ATemperature()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(Root);

	HeatSphere = CreateDefaultSubobject<USphereComponent>(TEXT("HeatSphere"));
	HeatSphere->SetupAttachment(Root);

	HeatSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HeatSphere->SetCollisionObjectType(ECC_WorldDynamic);
	HeatSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
	HeatSphere->SetGenerateOverlapEvents(true);

	HeatSphere->bDrawOnlyIfSelected = false;
	HeatSphere->ShapeColor = FColor::Red;
	HeatSphere->SetHiddenInGame(true);
	HeatSphere->SetVisibility(true);
}

void ATemperature::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateSphereRadius();
	UpdateVisuals();
}

void ATemperature::BeginPlay()
{
	Super::BeginPlay();

	UpdateSphereRadius();
	UpdateVisuals();

	if (HeatSphere)
	{
		HeatSphere->OnComponentBeginOverlap.AddDynamic(this, &ATemperature::OnSphereBeginOverlap);
		HeatSphere->OnComponentEndOverlap.AddDynamic(this, &ATemperature::OnSphereEndOverlap);
	}
}

void ATemperature::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CoolRate > 0.f)
	{
		Temperature = FMath::Max(0.f, Temperature - CoolRate * DeltaTime);
	}

	UpdateSphereRadius();
	UpdateVisuals();
}

float ATemperature::GetTotalRadiantPowerW() const
{
	const double T_K = static_cast<double>(Temperature) + 273.15;
	const double P = static_cast<double>(Emissivity) *
		static_cast<double>(StefanBoltzmannSigma) *
		static_cast<double>(SurfaceAreaM2) *
		FMath::Pow(T_K, 4.0);

	return static_cast<float>(P);
}

float ATemperature::GetHeatFluxWm2AtDistanceM(float DistanceM) const
{
	const float R = FMath::Max(DistanceM, 0.01f);

	const double P = static_cast<double>(GetTotalRadiantPowerW());
	const double Den = 4.0 * PI * static_cast<double>(R) * static_cast<double>(R);
	const double q = P / Den;

	return static_cast<float>(q);
}

float ATemperature::GetHeatFluxWm2AtLocation(const FVector& WorldLocation) const
{
	const double DistCm = FVector::Distance(GetActorLocation(), WorldLocation);
	const double DistM = DistCm / 100.0;

	if (MaxHeatDistance > 0.f && DistCm > MaxHeatDistance)
	{
		return 0.f;
	}

	return GetHeatFluxWm2AtDistanceM(static_cast<float>(DistM));
}

float ATemperature::GetReceivedPowerW(const FVector& WorldLocation, float ReceiverAreaM2) const
{
	const float q = GetHeatFluxWm2AtLocation(WorldLocation);
	return q * FMath::Max(ReceiverAreaM2, 0.f);
}

void ATemperature::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this) return;
	if (IceClassFilter && !OtherActor->IsA(IceClassFilter)) return;

	static const FName FnName(TEXT("StartHeating"));
	if (UFunction* Fn = OtherActor->FindFunction(FnName))
	{
		struct FArgs { ATemperature* FireRef; };
		FArgs Args{ this };
		OtherActor->ProcessEvent(Fn, &Args);
	}
}

void ATemperature::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (!OtherActor || OtherActor == this) return;
	if (IceClassFilter && !OtherActor->IsA(IceClassFilter)) return;

	static const FName FnName(TEXT("StopHeating"));
	if (UFunction* Fn = OtherActor->FindFunction(FnName))
	{
		OtherActor->ProcessEvent(Fn, nullptr);
	}
}

void ATemperature::UpdateSphereRadius()
{
	if (!HeatSphere) return;

	const float R = FMath::Max(0.f, MaxHeatDistance);
	HeatSphere->SetSphereRadius(R, true);
}

void ATemperature::UpdateVisuals()
{
	if (!MeshComp) return;

	if (bUseDynamicMaterial && HeatMaterial)
	{
		if (!HeatMID)
		{
			HeatMID = UMaterialInstanceDynamic::Create(HeatMaterial, this);
			MeshComp->SetMaterial(0, HeatMID);
		}
		if (HeatMID)
		{
			const float HeatAlpha = FMath::Clamp(Temperature * TempScale, 0.f, 1.f);
			HeatMID->SetScalarParameterValue(HeatAlphaParamName, HeatAlpha);
		}
	}

	if (bUseCPD)
	{
		MeshComp->SetCustomPrimitiveDataFloat(CPDIndex_Temperature, Temperature);
	}
}
