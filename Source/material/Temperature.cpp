#include "Temperature.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
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
	HeatSphere->SetCollisionProfileName(TEXT("Trigger"));
	HeatSphere->SetGenerateOverlapEvents(true);
	HeatSphere->bDrawOnlyIfSelected = false;
	HeatSphere->ShapeColor = FColor::Red;
	HeatSphere->SetHiddenInGame(true);
	HeatSphere->SetVisibility(true);
}

void ATemperature::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateSphereRadius(false);
	UpdateVisuals();
}

void ATemperature::BeginPlay()
{
	Super::BeginPlay();

	UpdateSphereRadius(true);
	UpdateVisuals();

	if (HeatSphere)
	{
		HeatSphere->OnComponentBeginOverlap.AddDynamic(this, &ATemperature::OnSphereBeginOverlap);
		HeatSphere->OnComponentEndOverlap.AddDynamic(this, &ATemperature::OnSphereEndOverlap);
		HeatSphere->UpdateOverlaps();
		EnsureOverlappingActorsHeating();
	}
}

void ATemperature::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CoolRate > 0.f)
	{
		Temperature = FMath::Max(0.f, Temperature - CoolRate * DeltaTime);
	}

	UpdateSphereRadius(false);
	UpdateVisuals();
	EnsureOverlappingActorsHeating();
}

float ATemperature::GetTotalRadiantPowerW() const
{
	const double T_K = static_cast<double>(Temperature) + 273.15;
	const double P = static_cast<double>(Emissivity)
		* static_cast<double>(StefanBoltzmannSigma)
		* static_cast<double>(SurfaceAreaM2)
		* FMath::Pow(T_K, 4.0);
	return static_cast<float>(P);
}

float ATemperature::GetHeatFluxWm2AtDistanceM(float DistanceM) const
{
	const double R = static_cast<double>(FMath::Max(DistanceM, 0.01f));
	const double P = static_cast<double>(GetTotalRadiantPowerW());
	return static_cast<float>(P / (4.0 * PI * R * R));
}

float ATemperature::GetHeatFluxWm2AtLocation(const FVector& WorldLocation) const
{
	const double DistCm = FVector::Distance(GetActorLocation(), WorldLocation);

	if (MaxHeatDistance > 0.f && DistCm > MaxHeatDistance)
	{
		return 0.f;
	}

	return GetHeatFluxWm2AtDistanceM(static_cast<float>(DistCm / 100.0));
}

float ATemperature::GetReceivedPowerW(const FVector& WorldLocation, float ReceiverAreaM2) const
{
	return GetHeatFluxWm2AtLocation(WorldLocation) * FMath::Max(ReceiverAreaM2, 0.f);
}

void ATemperature::OnSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this)
	{
		return;
	}
	if (IceClassFilter && !OtherActor->IsA(IceClassFilter))
	{
		return;
	}

	static const FName StartHeatingName(TEXT("StartHeating"));
	struct FArgs { ATemperature* FireRef; };
	FArgs Args{ this };
	CallFunctionOnActor(OtherActor, StartHeatingName, &Args);
}

void ATemperature::OnSphereEndOverlap(
	UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor || OtherActor == this)
	{
		return;
	}
	if (IceClassFilter && !OtherActor->IsA(IceClassFilter))
	{
		return;
	}

	static const FName StopHeatingName(TEXT("StopHeating"));
	CallFunctionOnActor(OtherActor, StopHeatingName);
}

void ATemperature::UpdateSphereRadius(bool bForceOverlaps)
{
	if (!HeatSphere)
	{
		return;
	}

	const float R = FMath::Max(0.f, MaxHeatDistance);
	const bool bChanged = !FMath::IsNearlyEqual(R, LastSphereRadius, 0.01f);

	if (bChanged)
	{
		HeatSphere->SetSphereRadius(R, true);
		LastSphereRadius = R;
	}

	if (bForceOverlaps || bChanged)
	{
		HeatSphere->UpdateOverlaps();
	}
}

void ATemperature::UpdateVisuals()
{
	if (!MeshComp)
	{
		return;
	}

	if (bUseDynamicMaterial && HeatMaterial)
	{
		if (!HeatMID)
		{
			HeatMID = UMaterialInstanceDynamic::Create(HeatMaterial, this);
			MeshComp->SetMaterial(0, HeatMID);
		}
		if (HeatMID)
		{
			HeatMID->SetScalarParameterValue(HeatAlphaParamName,
				FMath::Clamp(Temperature * TempScale, 0.f, 1.f));
		}
	}

	if (bUseCPD)
	{
		MeshComp->SetCustomPrimitiveDataFloat(CPDIndex_Temperature, Temperature);
	}
}

void ATemperature::EnsureOverlappingActorsHeating()
{
	if (!HeatSphere)
	{
		return;
	}

	TArray<AActor*> Actors;
	GetFilteredOverlappingActors(Actors);

	static const FName StartHeatingName(TEXT("StartHeating"));
	static const FName IsHeatingName(TEXT("IsHeating"));

	for (AActor* Actor : Actors)
	{
		bool bAlreadyHeating = false;
		if (UFunction* CheckFn = Actor->FindFunction(IsHeatingName))
		{
			Actor->ProcessEvent(CheckFn, &bAlreadyHeating);
		}

		if (!bAlreadyHeating)
		{
			struct FArgs { ATemperature* FireRef; };
			FArgs Args{ this };
			CallFunctionOnActor(Actor, StartHeatingName, &Args);
		}
	}
}

void ATemperature::CallFunctionOnActor(AActor* Target, FName FunctionName, void* Params) const
{
	if (UFunction* Fn = Target->FindFunction(FunctionName))
	{
		Target->ProcessEvent(Fn, Params);
	}
}

void ATemperature::GetFilteredOverlappingActors(TArray<AActor*>& OutActors) const
{
	if (IceClassFilter)
	{
		HeatSphere->GetOverlappingActors(OutActors, IceClassFilter);
	}
	else
	{
		HeatSphere->GetOverlappingActors(OutActors);
	}

	OutActors.RemoveAll([this](const AActor* A)
	{
		if (!A || A == this)
		{
			return true;
		}
		return IceClassFilter && !A->IsA(IceClassFilter);
	});
}