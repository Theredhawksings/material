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
    DefaultSpec.MaterialTag = TEXT("Metal");
    DefaultSpec.ChargeAmount = 3;
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

    SetActorRelativeLocation(FVector(0.04f, 0.f, 0.07f));
    SetActorRelativeRotation(FRotator(90.f, 90.f, 0.f));
    
    bIsAttached = true;
    Character->SetAttachedSyringe(this);
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
}