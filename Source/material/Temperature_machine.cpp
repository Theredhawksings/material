#include "Temperature_machine.h"
#include "Temperature.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

ATemperature_machine::ATemperature_machine()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(Root);

	DetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionBox"));
	DetectionBox->SetupAttachment(Root);
	DetectionBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	DetectionBox->SetGenerateOverlapEvents(true);

	DetectionBox->SetBoxExtent(BoxExtent);
	DetectionBox->SetRelativeLocation(BoxRelativeLocation);

	DetectionBox->SetHiddenInGame(false);
	DetectionBox->SetVisibility(true);
	DetectionBox->ShapeColor = FColor::Orange;
}

void ATemperature_machine::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!MeshComp || !DetectionBox) return;

	const FVector CurrentBoxExtent = DetectionBox->GetUnscaledBoxExtent();
	const float MeshHeight = MeshComp->Bounds.BoxExtent.Z;
	const float ZPosition = MeshHeight + CurrentBoxExtent.Z;

	DetectionBox->SetRelativeLocation(
		FVector(
			BoxRelativeLocation.X,
			BoxRelativeLocation.Y,
			ZPosition + BoxRelativeLocation.Z
		)
	);
}

void ATemperature_machine::BeginPlay()
{
	Super::BeginPlay();

	if (DetectionBox)
	{
		DetectionBox->OnComponentBeginOverlap.AddDynamic(
			this, &ATemperature_machine::OnBoxBeginOverlap
		);
		DetectionBox->OnComponentEndOverlap.AddDynamic(
			this, &ATemperature_machine::OnBoxEndOverlap
		);

		TArray<AActor*> TempActors;
		DetectionBox->GetOverlappingActors(TempActors, ATemperature::StaticClass());

		for (AActor* Actor : TempActors)
		{
			if (ATemperature* TempActor = Cast<ATemperature>(Actor))
			{
				OverlappingTemperatureActors.AddUnique(TempActor);
			}
		}
	}
}

void ATemperature_machine::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 머신 상태 항상 표시
	GEngine->AddOnScreenDebugMessage(
		static_cast<int32>(GetUniqueID()) + 2000,
		0.f,
		bIsActive ? FColor::Green : FColor::Red,
		FString::Printf(TEXT("[%s] Active: %s | Overlapping: %d"),
			*GetName(),
			bIsActive ? TEXT("ON") : TEXT("OFF"),
			OverlappingTemperatureActors.Num())
	);

	if (bIsActive && OverlappingTemperatureActors.Num() > 0)
	{
		ApplyConductionHeatTransfer(DeltaTime);
	}
}

void ATemperature_machine::OnBoxBeginOverlap(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this) return;

	if (ATemperature* TempActor = Cast<ATemperature>(OtherActor))
	{
		OverlappingTemperatureActors.AddUnique(TempActor);
	}
}

void ATemperature_machine::OnBoxEndOverlap(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (!OtherActor || OtherActor == this) return;

	if (ATemperature* TempActor = Cast<ATemperature>(OtherActor))
	{
		OverlappingTemperatureActors.Remove(TempActor);
	}
}

void ATemperature_machine::ApplyConductionHeatTransfer(float DeltaTime)
{
	OverlappingTemperatureActors.RemoveAll(
		[](ATemperature* Actor)
		{
			return Actor == nullptr || !IsValid(Actor);
		}
	);

	for (ATemperature* TempActor : OverlappingTemperatureActors)
	{
		if (!TempActor) continue;

		const float ObjectTemp = TempActor->Temperature;
		const float HeatTransferRate = CalculateHeatTransferRate(ObjectTemp);
		const float TempChange =
			(HeatTransferRate * DeltaTime) /
			(ObjectMassKg * SpecificHeatCapacity);

		TempActor->Temperature = ObjectTemp + TempChange;
	}
}

float ATemperature_machine::CalculateHeatTransferRate(float ObjectTemperature) const
{
	const float TemperatureDifference = SurfaceTemperature - ObjectTemperature;

	float HeatTransferRate =
		(ThermalConductivity * ContactAreaM2 * TemperatureDifference) /
		FMath::Max(ConductionThicknessM, 0.0001f);

	return HeatTransferRate * EfficiencyFactor;
}

void ATemperature_machine::SetMachineActive(bool bActive)
{
	bIsActive = bActive;
}

void ATemperature_machine::ToggleMachine()
{
	SetMachineActive(!bIsActive);
}
