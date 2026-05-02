#include "Syringe.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "materialCharacter.h"

ASyringe::ASyringe()
{
    PrimaryActorTick.bCanEverTick = true;

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

    static ConstructorHelpers::FObjectFinder<UMaterial> MetalMatFinder(
        TEXT("/Script/Engine.Material'/Game/modeling/Object/Battery/M_Battery_Metal.M_Battery_Metal'"));
    if (MetalMatFinder.Succeeded()) MaterialMap.Add(ESyringeMaterial::Metal, MetalMatFinder.Object);

    static ConstructorHelpers::FObjectFinder<UMaterial> CopperMatFinder(
        TEXT("/Script/Engine.Material'/Game/modeling/Object/Battery/M_Battery_Copper.M_Battery_Copper'"));
    if (CopperMatFinder.Succeeded()) MaterialMap.Add(ESyringeMaterial::Copper, CopperMatFinder.Object);

    static ConstructorHelpers::FObjectFinder<UMaterial> RubberMatFinder(
        TEXT("/Script/Engine.Material'/Game/modeling/Object/Battery/M_Battery_Rubber.M_Battery_Rubber'"));
    if (RubberMatFinder.Succeeded()) MaterialMap.Add(ESyringeMaterial::Rubber, RubberMatFinder.Object);

    static ConstructorHelpers::FObjectFinder<UMaterial> IceMatFinder(
        TEXT("/Script/Engine.Material'/Game/modeling/Object/Battery/M_Battery_Ice.M_Battery_Ice'"));
    if (IceMatFinder.Succeeded()) MaterialMap.Add(ESyringeMaterial::Ice, IceMatFinder.Object);

    static ConstructorHelpers::FObjectFinder<UMaterial> WoodMatFinder(
        TEXT("/Script/Engine.Material'/Game/modeling/Object/Battery/M_Battery_Wood.M_Battery_Wood'"));
    if (WoodMatFinder.Succeeded()) MaterialMap.Add(ESyringeMaterial::Wood, WoodMatFinder.Object);

    static ConstructorHelpers::FObjectFinder<UMaterial> MagnetMatFinder(
        TEXT("/Script/Engine.Material'/Game/modeling/Object/Battery/M_Battery_Magnet.M_Battery_Magnet'"));
    if (MagnetMatFinder.Succeeded()) MaterialMap.Add(ESyringeMaterial::Magnet, MagnetMatFinder.Object);

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
    ApplyMaterialByType();
}

void ASyringe::ApplyMaterialByType()
{
    if (!MeshComp || ChargeSpecs.Num() == 0) return;

    ESyringeMaterial Type = ChargeSpecs[0].MaterialType;
    if (TObjectPtr<UMaterialInterface>* FoundMat = MaterialMap.Find(Type))
    {
        if (*FoundMat)
            MeshComp->SetMaterial(0, *FoundMat);
    }
}

void ASyringe::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsRotating) return;

    RotAnimTime += DeltaTime;
    float Alpha = FMath::Clamp(RotAnimTime / RotAnimDuration, 0.f, 1.f);
    float SmoothAlpha = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);

    FQuat StartQuat(RotAnimStartCached);
    FQuat EndQuat(RotAnimEndRot);
    FQuat ResultQuat = FQuat::Slerp(StartQuat, EndQuat, SmoothAlpha);

    SetActorRelativeRotation(ResultQuat);

    if (Alpha >= 1.f) bIsRotating = false;
}

void ASyringe::StartRotationAnim()
{
    if (USceneComponent* Root = GetRootComponent())
        RotAnimStartCached = Root->GetRelativeRotation();
    RotAnimTime = 0.f;
    bIsRotating = true;
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

    SetActorRelativeLocation(FVector(0.04f, 0.03f, 0.0f));
    SetActorRelativeRotation(FRotator(270.f, 90.f, 0.f));
    
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