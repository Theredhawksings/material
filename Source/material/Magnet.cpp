// Magnet.cpp

#include "Magnet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"

AMagnet::AMagnet()
{
    PrimaryActorTick.bCanEverTick = true;

    /* Root Mesh */
    MagnetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MagnetMesh"));
    RootComponent = MagnetMesh;
    MagnetMesh->SetSimulatePhysics(false);

    /* Magnetic Range */
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

    // Strength = m_max * g * r0^2
    if (bAutoComputeStrength)
    {
        const float g = 980.f; // cm/s^2
        Strength = MaxLiftMass * g * FMath::Square(ReferenceDistance);
    }

    UE_LOG(LogTemp, Warning, TEXT("[Magnet] Strength = %e"), Strength);

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

        // F = Strength / r^2
        float ForceMag = Strength / FMath::Square(Distance);

        // 🔥 핵심: DeltaTime 곱지 않는다 (AddForce는 누적)
        FVector Force = Dir * ForceMag;

        // 폭주 방지
        const float MaxForce = 1e6f;
        Force = Force.GetClampedToMaxSize(MaxForce);

        MetalComp->AddForce(Force);

        UE_LOG(LogTemp, Warning,
            TEXT("Pulling %s | Dist=%.1f | Force=%.2e"),
            *MetalComp->GetName(), Distance, ForceMag);
    }
}

void AMagnet::OnRangeBegin(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult
)
{
    if (!OtherActor || OtherActor == this || !OtherComp)
        return;

    if (OtherActor->ActorHasTag(MetalTag) && OtherComp->IsSimulatingPhysics())
    {
        OverlappingMetals.Add(OtherComp);
        UE_LOG(LogTemp, Warning, TEXT("[Magnet] Metal Entered"));
    }
}

void AMagnet::OnRangeEnd(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex
)
{
    if (!OtherActor || !OtherComp)
        return;

    if (OtherActor->ActorHasTag(MetalTag))
    {
        OverlappingMetals.Remove(OtherComp);
        UE_LOG(LogTemp, Warning, TEXT("[Magnet] Metal Left"));
    }
}
