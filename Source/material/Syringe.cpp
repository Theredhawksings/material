#include "Syringe.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "materialCharacter.h"

ASyringe::ASyringe()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetMobility(EComponentMobility::Movable);
    MeshComp->SetCollisionProfileName(TEXT("NoCollision"));
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
    OverlapComp->SetSphereRadius(150.f);
    OverlapComp->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    OverlapComp->OnComponentBeginOverlap.AddDynamic(this, &ASyringe::OnOverlapBegin);

    FSyringeChargeSpec DefaultSpec;
    DefaultSpec.MaterialType = ESyringeMaterial::Metal;
    DefaultSpec.ChargeAmount = 1;
    ChargeSpecs.Add(DefaultSpec);
}

void ASyringe::BeginPlay()
{
    Super::BeginPlay();
}

void ASyringe::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || bIsAttached) return;

    AmaterialCharacter* Character = Cast<AmaterialCharacter>(OtherActor);
    if (!Character) return;

    if (Character->GetAttachedSyringe() != nullptr) return;

    UE_LOG(LogTemp, Warning, TEXT("Character Detected: %s"), *OtherActor->GetName());

    AttachToCharacterHand(Character);
}

void ASyringe::AttachToCharacterHand(AmaterialCharacter* Character)
{
    if (!Character) return;

    OverlapComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComp->SetWorldScale3D(FVector(0.15f, 0.15f, 0.15f));

    FName SocketName = TEXT("hand_L_endSocket");
    FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);
    AttachToComponent(Character->GetMesh(), AttachRules, SocketName);

    SetActorRelativeLocation(FVector(0.04f, 0.1f, 0.05f));
    SetActorRelativeRotation(FRotator(90.f, 90.f, 0.f));
    
    bIsAttached = true;
    Character->SetAttachedSyringe(this);
}

void ASyringe::UseSyringe(AmaterialCharacter* Character)
{
	if (!Character || bIsUsed) return;

	for (const FSyringeChargeSpec& Spec : ChargeSpecs)
	{
		if (Spec.ChargeAmount <= 0) continue;
		FName Tag = MaterialEnumToTag(Spec.MaterialType);
		Character->ChargeGaugeForMaterial(Tag, Spec.ChargeAmount);
	}

	bIsUsed = true;
}

FName ASyringe::MaterialEnumToTag(ESyringeMaterial Material)
{
	switch (Material)
	{
		case ESyringeMaterial::Metal:  return TEXT("Metal");
		case ESyringeMaterial::Copper: return TEXT("Copper");
		case ESyringeMaterial::Rubber: return TEXT("Rubber");
		case ESyringeMaterial::Ice:    return TEXT("Ice");
		case ESyringeMaterial::Wood:   return TEXT("Wood");
		case ESyringeMaterial::Magnet: return TEXT("Magnet");
	}
	return TEXT("Metal");
}