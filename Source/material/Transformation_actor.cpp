#include "Transformation_actor.h"

#include "Components/StaticMeshComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

ATransformation_actor::ATransformation_actor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	SetRootComponent(MeshComp);

	MeshComp->SetMobility(EComponentMobility::Movable);
	MeshComp->SetCollisionProfileName(TEXT("PhysicsActor"));
	MeshComp->SetGenerateOverlapEvents(true);
}

void ATransformation_actor::BeginPlay()
{
	Super::BeginPlay();

	SetForm(CurrentForm);
}

void ATransformation_actor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	SetForm(CurrentForm);
}

void ATransformation_actor::SetForm(EBlockForm NewForm)
{
	CurrentForm = NewForm;

	if (const FBlockFormSpec* Spec = FindSpec(CurrentForm))
	{
		ApplySpec(*Spec);
	}
}

void ATransformation_actor::CycleForm()
{
	if (CycleOrder.Num() <= 0)
	{
		return;
	}

	int32 Idx = CycleOrder.Find(CurrentForm);
	if (Idx == INDEX_NONE)
	{
		SetForm(CycleOrder[0]);
		return;
	}

	Idx = (Idx + 1) % CycleOrder.Num();
	SetForm(CycleOrder[Idx]);
}

bool ATransformation_actor::GetCurrentSpec(FBlockFormSpec& OutSpec) const
{
	if (const FBlockFormSpec* Spec = FindSpec(CurrentForm))
	{
		OutSpec = *Spec;
		return true;
	}
	return false;
}

const FBlockFormSpec* ATransformation_actor::FindSpec(EBlockForm Form) const
{
	for (const FBlockFormSpec& S : FormSpecs)
	{
		if (S.Form == Form)
		{
			return &S;
		}
	}
	return nullptr;
}

void ATransformation_actor::ApplySpec(const FBlockFormSpec& Spec)
{
	if (!MeshComp)
	{
		return;
	}

	if (Spec.Mesh)
	{
		MeshComp->SetStaticMesh(Spec.Mesh);
	}

	if (Spec.Materials.Num() > 0)
	{
		for (int32 i = 0; i < Spec.Materials.Num(); ++i)
		{
			if (Spec.Materials[i])
			{
				MeshComp->SetMaterial(i, Spec.Materials[i]);
			}
		}
	}

	MeshComp->SetSimulatePhysics(Spec.bSimulatePhysics);

	MeshComp->SetLinearDamping(Spec.LinearDamping);
	MeshComp->SetAngularDamping(Spec.AngularDamping);

	if (Spec.PhysMat)
	{
		MeshComp->SetPhysMaterialOverride(Spec.PhysMat);
	}

	if (Spec.bOverrideMass)
	{
		MeshComp->SetMassOverrideInKg(NAME_None, Spec.MassKg, true);
	}
}
