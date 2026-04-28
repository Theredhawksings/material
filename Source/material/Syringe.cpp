#include "Syringe.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "materialCharacter.h"

ASyringe::ASyringe()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	SetRootComponent(MeshComp);
	MeshComp->SetMobility(EComponentMobility::Movable);
	MeshComp->SetCollisionProfileName(TEXT("BlockAll"));
	MeshComp->SetSimulatePhysics(false);
	MeshComp->SetEnableGravity(false);
	MeshComp->SetWorldScale3D(FVector(40.f, 40.f, 40.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SyringeMesh(
		TEXT("/Script/Engine.StaticMesh'/Game/modeling/Object/Battery/Battery.Battery'"));
	if (SyringeMesh.Succeeded())
		MeshComp->SetStaticMesh(SyringeMesh.Object);

	static ConstructorHelpers::FObjectFinder<UMaterial> SyringeMat(
		TEXT("/Script/Engine.Material'/Game/modeling/Object/Battery/M_Battery.M_Battery'"));
	if (SyringeMat.Succeeded())
		MeshComp->SetMaterial(0, SyringeMat.Object);

	OverlapComp = CreateDefaultSubobject<USphereComponent>(TEXT("OverlapComp"));
	OverlapComp->SetupAttachment(RootComponent);
	OverlapComp->SetSphereRadius(OverlapRadius);
	OverlapComp->SetCollisionProfileName(TEXT("Trigger"));
	OverlapComp->OnComponentBeginOverlap.AddDynamic(this, &ASyringe::OnOverlapBegin);

	FSyringeChargeSpec DefaultSpec;
	DefaultSpec.MaterialTag = TEXT("Metal");
	DefaultSpec.ChargeAmount = 3;
	ChargeSpecs.Add(DefaultSpec);
}

void ASyringe::BeginPlay()
{
	Super::BeginPlay();
	SetActorEnableCollision(true);
}

void ASyringe::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (bIsUsed) return;

	AmaterialCharacter* Character = Cast<AmaterialCharacter>(OtherActor);
	if (!Character) return;

	UseSyringe(Character);
}

void ASyringe::AttachToCharacterHand(AmaterialCharacter* Character)
{
	// 제자리 고정이므로 사용 안 함
}

void ASyringe::UseSyringe(AmaterialCharacter* Character)
{
	if (!Character || bIsUsed) return;

	for (const FSyringeChargeSpec& Spec : ChargeSpecs)
	{
		if (Spec.MaterialTag.IsNone() || Spec.ChargeAmount <= 0) continue;
		Character->ChargeGaugeForMaterial(Spec.MaterialTag, Spec.ChargeAmount);
	}

	bIsUsed = true;
	Destroy();
}