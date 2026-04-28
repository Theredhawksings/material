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
    OverlapComp->SetCollisionProfileName(TEXT("Trigger"));
    OverlapComp->OnComponentBeginOverlap.AddDynamic(this, &ASyringe::OnOverlapBegin);
}

void ASyringe::BeginPlay()
{
    Super::BeginPlay();
}

void ASyringe::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor) return;

    AmaterialCharacter* Character = Cast<AmaterialCharacter>(OtherActor);
    if (!Character) return;

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

    //SetActorRelativeLocation(FVector(10.f, 5.f, -5.f));
    SetActorRelativeRotation(FRotator(90.f, 0.f, 0.f));
}

void ASyringe::UseSyringe(AmaterialCharacter* Character)
{
    if (!Character) return;
}