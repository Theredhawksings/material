#include "Magnet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"

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
    MagneticDecayExponent = 2.0f;
    VelocityDampingFactor = 0.3f;
    MaxAttractVelocity = 500.f;
    bUseTorque = true;
}

void AMagnet::BeginPlay()
{
    Super::BeginPlay();

    if (bAutoComputeStrength)
    {
        const float g = 980.f;
        Strength = MaxLiftMass * g * FMath::Pow(ReferenceDistance, MagneticDecayExponent);
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
    const FVector MagnetForward = MagnetMesh->GetForwardVector();

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
        
        float DirectionFactor = FVector::DotProduct(Dir, MagnetForward);
        DirectionFactor = FMath::Lerp(0.5f, 1.0f, (DirectionFactor + 1.0f) * 0.5f);
        
        float ForceMag = (Strength * DirectionFactor) / FMath::Pow(Distance, MagneticDecayExponent);

        float MetalMass = MetalComp->GetMass();
        float MassScale = FMath::Clamp(MetalMass / 10.0f, 0.1f, 2.0f);
        ForceMag *= MassScale;

        FVector CurrentVel = MetalComp->GetPhysicsLinearVelocity();
        float VelTowardsMagnet = FVector::DotProduct(CurrentVel, Dir);
        
        float VelocityDamping = 1.0f;
        if (VelTowardsMagnet > MaxAttractVelocity * 0.5f)
        {
            VelocityDamping = FMath::Clamp(1.0f - (VelTowardsMagnet / MaxAttractVelocity), 0.2f, 1.0f);
        }
        
        FVector DampingForce = -CurrentVel * VelocityDampingFactor * MetalMass;

        FVector FinalForce = (Dir * ForceMag * VelocityDamping) + DampingForce;
        const float MaxForce = 1e6f;
        FinalForce = FinalForce.GetClampedToMaxSize(MaxForce);

        MetalComp->AddForce(FinalForce, NAME_None, false);

        if (bUseTorque)
        {
            FVector MetalForward = MetalComp->GetForwardVector();
            FVector CrossProduct = FVector::CrossProduct(MetalForward, Dir);
            float TorqueMagnitude = CrossProduct.Size() * ForceMag * 0.1f;
            
            if (TorqueMagnitude > 0.01f)
            {
                FVector TorqueDir = CrossProduct.GetSafeNormal();
                MetalComp->AddTorqueInRadians(TorqueDir * TorqueMagnitude, NAME_None, false);
            }
        }

        if (MagnetMesh->IsSimulatingPhysics())
        {
            MagnetMesh->AddForce(-FinalForce * 0.5f, NAME_None, false);
        }

        if (bDebugDraw)
        {
            DrawDebugLine(GetWorld(), MetalLoc, MagnetLoc, FColor::Cyan, false, -1.f, 0, 2.f);
            DrawDebugSphere(GetWorld(), MetalLoc, 20.f, 8, FColor::Red, false, -1.f);
        }
    }
}

void AMagnet::OnRangeBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == this || !OtherComp)
        return;

    if (OtherActor->ActorHasTag(MetalTag) && OtherComp->IsSimulatingPhysics())
    {
        OverlappingMetals.Add(OtherComp);
        
        if (bApplyInitialImpulse)
        {
            FVector ToMagnet = (MagnetMesh->GetComponentLocation() - OtherComp->GetComponentLocation()).GetSafeNormal();
            OtherComp->AddImpulse(ToMagnet * InitialImpulseStrength * OtherComp->GetMass());
        }
    }
}

void AMagnet::OnRangeEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!OtherActor || !OtherComp)
        return;

    if (OtherActor->ActorHasTag(MetalTag))
    {
        OverlappingMetals.Remove(OtherComp);
    }
}