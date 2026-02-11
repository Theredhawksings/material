// Magnet.cpp - 적당하게 조정 + N값만 표시
#include "Magnet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"

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
}

void AMagnet::BeginPlay()
{
    Super::BeginPlay();

    if (bAutoComputeStrength)
    {
        const float g = 980.f;
        Strength = MaxLiftMass * g * FMath::Pow(ReferenceDistance, MagneticDecayExponent);
    }

    MagnetRange->SetSphereRadius(MaxDistance);
    MagnetRange->OnComponentBeginOverlap.AddDynamic(this, &AMagnet::OnRangeBegin);
    MagnetRange->OnComponentEndOverlap.AddDynamic(this, &AMagnet::OnRangeEnd);

    RefreshOverlappingMetals();
}

void AMagnet::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    TimeSinceLastRefresh += DeltaTime;
    if (TimeSinceLastRefresh >= RefreshInterval)
    {
        TimeSinceLastRefresh = 0.f;
        RefreshOverlappingMetals();
    }

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
        DirectionFactor = FMath::Lerp(0.75f, 1.0f, (DirectionFactor + 1.0f) * 0.5f);
        
        float ForceMag = (Strength * DirectionFactor * ForceMultiplier) / FMath::Pow(Distance, MagneticDecayExponent);

        float MetalMass = MetalComp->GetMass();
        float MassScale = FMath::Clamp(MetalMass / 5.0f, 0.6f, 2.5f);
        ForceMag *= MassScale;

        FVector CurrentVel = MetalComp->GetPhysicsLinearVelocity();
        float VelTowardsMagnet = FVector::DotProduct(CurrentVel, Dir);
        
        float VelocityDamping = 1.0f;
        if (VelTowardsMagnet > MaxAttractVelocity * 0.7f)
        {
            VelocityDamping = FMath::Clamp(1.0f - (VelTowardsMagnet / MaxAttractVelocity), 0.4f, 1.0f);
        }
        
        FVector DampingForce = -CurrentVel * VelocityDampingFactor * MetalMass;

        FVector FinalForce = (Dir * ForceMag * VelocityDamping) + DampingForce;
        const float MaxForce = 6e7f;
        FinalForce = FinalForce.GetClampedToMaxSize(MaxForce);

        MetalComp->AddForce(FinalForce, NAME_None, false);

        if (bUseTorque)
        {
            FVector MetalForward = MetalComp->GetForwardVector();
            FVector CrossProduct = FVector::CrossProduct(MetalForward, Dir);
            float TorqueMagnitude = CrossProduct.Size() * ForceMag * 0.3f;
            
            if (TorqueMagnitude > 0.01f)
            {
                FVector TorqueDir = CrossProduct.GetSafeNormal();
                MetalComp->AddTorqueInRadians(TorqueDir * TorqueMagnitude, NAME_None, false);
            }
        }

        if (MagnetMesh->IsSimulatingPhysics())
        {
            MagnetMesh->AddForce(-FinalForce * 0.2f, NAME_None, false);
        }

        if (bDebugDraw)
        {
            DrawDebugLine(GetWorld(), MetalLoc, MagnetLoc, FColor::Cyan, false, -1.f, 0, 2.f);
            DrawDebugSphere(GetWorld(), MetalLoc, 25.f, 8, FColor::Red, false, -1.f);
            
            FString ForceInfo = FString::Printf(TEXT("%.0f N"), FinalForce.Size());
            DrawDebugString(GetWorld(), MetalLoc + FVector(0, 0, 50), ForceInfo, nullptr, FColor::Yellow, 0.0f);
        }
    }

    if (bEnableInduction && OverlappingMetals.Num() > 0)
    {
        ApplyInducedMagnetism();
    }

}

void AMagnet::RefreshOverlappingMetals()
{
    if (!MagnetRange)
        return;

    TArray<UPrimitiveComponent*> OverlappingComps;
    MagnetRange->GetOverlappingComponents(OverlappingComps);

    OverlappingMetals.Empty();

    for (UPrimitiveComponent* Comp : OverlappingComps)
    {
        if (!Comp || !Comp->IsSimulatingPhysics())
            continue;

        AActor* CompOwner = Comp->GetOwner();
        if (CompOwner && CompOwner != this && CompOwner->ActorHasTag(MetalTag))
        {
            OverlappingMetals.Add(Comp);
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

void AMagnet::ApplyInducedMagnetism()
{
    const FVector MagnetLoc = MagnetMesh->GetComponentLocation();
    
    TArray<UPrimitiveComponent*> MetalArray = OverlappingMetals.Array();
    
    for (int32 i = 0; i < MetalArray.Num(); ++i)
    {
        UPrimitiveComponent* MetalA = MetalArray[i];
        if (!IsValid(MetalA) || !MetalA->IsSimulatingPhysics())
            continue;

        const FVector MetalALoc = MetalA->GetComponentLocation();
        float DistAToMagnet = FVector::Dist(MetalALoc, MagnetLoc);

        if (DistAToMagnet > MinDistanceForInduction)
            continue;
        
        float InducedStrength = CalculateInducedStrength(DistAToMagnet, Strength);

        for (int32 j = 0; j < MetalArray.Num(); ++j)
        {
            if (i == j) continue; 
            
            UPrimitiveComponent* MetalB = MetalArray[j];
            if (!IsValid(MetalB) || !MetalB->IsSimulatingPhysics())
                continue;

            const FVector MetalBLoc = MetalB->GetComponentLocation();
            FVector AtoB = MetalBLoc - MetalALoc;
            float DistAtoB = AtoB.Size();

            if (DistAtoB < 10.f || DistAtoB > InductionRange)
                continue;
            
            FVector Dir = AtoB.GetSafeNormal();

            FVector MagnetToA = (MetalALoc - MagnetLoc).GetSafeNormal();
            
            float AlignmentFactor = FVector::DotProduct(Dir, MagnetToA);
            
            float DirectionMult = FMath::Sign(AlignmentFactor) * FMath::Abs(AlignmentFactor);

            float ForceMag = (InducedStrength * InductionStrengthRatio * FMath::Abs(DirectionMult)) 
                           / FMath::Pow(DistAtoB, MagneticDecayExponent);
            
            float MetalBMass = MetalB->GetMass();
            float MassScale = FMath::Clamp(MetalBMass / 10.0f, 0.5f, 2.0f);
            ForceMag *= MassScale;

            FVector CurrentVel = MetalB->GetPhysicsLinearVelocity();
            float VelTowardsA = FVector::DotProduct(CurrentVel, Dir);
            float VelocityDamping = 1.0f;
            if (VelTowardsA > MaxAttractVelocity * 0.5f)
            {
                VelocityDamping = FMath::Clamp(1.0f - (VelTowardsA / MaxAttractVelocity), 0.3f, 1.0f);
            }
            
            FVector DampingForce = -CurrentVel * VelocityDampingFactor * 0.5f * MetalBMass;
            FVector FinalForce = (Dir * ForceMag * VelocityDamping * DirectionMult) + DampingForce;
            
            const float MaxInducedForce = 3e7f;
            FinalForce = FinalForce.GetClampedToMaxSize(MaxInducedForce);
            
            MetalB->AddForce(FinalForce, NAME_None, false);
            
            MetalA->AddForce(-FinalForce * 0.5f, NAME_None, false);
            
            if (bDebugDraw)
            {
                DrawDebugLine(GetWorld(), MetalALoc, MetalBLoc, 
                    FColor::Yellow, false, -1.f, 0, 1.f);
            }
        }
    }
}

float AMagnet::CalculateInducedStrength(float DistanceToMagnet, float BaseMagnetStrength) const
{
    if (DistanceToMagnet < 1.0f)
        DistanceToMagnet = 1.0f;
    
    float InductionFactor = 1.0f / FMath::Pow(DistanceToMagnet / MinDistanceForInduction, 1.5f);
    InductionFactor = FMath::Clamp(InductionFactor, 0.0f, 1.0f);
    
    return BaseMagnetStrength * InductionFactor;
}