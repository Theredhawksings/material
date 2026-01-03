#include "Magnet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"

AMagnet::AMagnet()
{
    PrimaryActorTick.bCanEverTick = true;

    MagnetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MagnetMesh"));
    RootComponent = MagnetMesh;
    MagnetMesh->SetSimulatePhysics(false);

    MagnetRange = CreateDefaultSubobject<USphereComponent>(TEXT("MagnetRange"));
    MagnetRange->SetupAttachment(MagnetMesh);
    MagnetRange->SetSphereRadius(MaxDistance);
    MagnetRange->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MagnetRange->SetCollisionResponseToAllChannels(ECR_Ignore);
    MagnetRange->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);

    Strength = 0.f;
}

void AMagnet::BeginPlay()
{
    Super::BeginPlay();

    if (bAutoComputeStrength)
    {
        const float g = 980.f;
        Strength = MaxLiftMass * g * FMath::Square(ReferenceDistance);
    }

    MagnetRange->OnComponentBeginOverlap.AddDynamic(this, &AMagnet::OnRangeBegin);
    MagnetRange->OnComponentEndOverlap.AddDynamic(this, &AMagnet::OnRangeEnd);
}

void AMagnet::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (OverlappingMetals.Num() == 0)
        return;

    const FVector MagnetLoc = MagnetMesh->GetComponentLocation();

    for (UPrimitiveComponent* MetalComp : OverlappingMetals)
    {
        if (!IsValid(MetalComp) || !MetalComp->IsSimulatingPhysics())
            continue;

        const FVector MetalLoc = MetalComp->GetComponentLocation();
        FVector ToMagnet = MagnetLoc - MetalLoc;
        float Distance = ToMagnet.Size();

        if (Distance < MinDistance || Distance > MaxDistance)
            continue;

        FVector Dir = ToMagnet.GetSafeNormal();
        float ForceMag = Strength / (FMath::Square(Distance) + 100.0f);

        FVector CurrentVel = MetalComp->GetPhysicsLinearVelocity();
        FVector DampingForce = -CurrentVel * 0.5f;

        FVector FinalForce = (Dir * ForceMag) + DampingForce;
        const float MaxForce = 1e6f;
        FinalForce = FinalForce.GetClampedToMaxSize(MaxForce);

        MetalComp->AddForce(FinalForce);

        if (MagnetMesh->IsSimulatingPhysics())
        {
            MagnetMesh->AddForce(-FinalForce);
        }
    }
}

void AMagnet::OnRangeBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == this || !OtherComp)
        return;

    if (OtherActor->ActorHasTag(MetalTag) && OtherComp->IsSimulatingPhysics())
    {
        OverlappingMetals.Add(OtherComp);
    }
}

void AMagnet::OnRangeEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!OtherActor || !OtherComp)
        return;

    if (OtherActor->ActorHasTag(MetalTag))
    {
        OverlappingMetals.Remove(OtherComp);
    }
}