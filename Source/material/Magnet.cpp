#include "Magnet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Temperature.h"
#include "Kismet/GameplayStatics.h"

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
        Strength = MaxLiftMass * GravityAccel * FMath::Pow(ReferenceDistance, MagneticDecayExponent);
    }

    MagnetRange->SetSphereRadius(MaxDistance);
    MagnetRange->OnComponentBeginOverlap.AddDynamic(this, &AMagnet::OnRangeBegin);
    MagnetRange->OnComponentEndOverlap.AddDynamic(this, &AMagnet::OnRangeEnd);

    CheckDemagnetize();

    if (!bDemagnetized)
    {
        RefreshOverlappingMetals();
    }
}

void AMagnet::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bDemagnetized)
    {
        return;
    }

    TimeSinceLastRefresh += DeltaTime;
    if (TimeSinceLastRefresh >= RefreshInterval)
    {
        TimeSinceLastRefresh = 0.f;

        CheckDemagnetize();
        if (bDemagnetized)
        {
            OverlappingMetals.Empty();
            return;
        }

        RefreshOverlappingMetals();
    }

    if (OverlappingMetals.Num() == 0)
    {
        return;
    }

    const FVector MagnetLoc = MagnetMesh->GetComponentLocation();
    const FVector MagnetForward = MagnetMesh->GetForwardVector();
    const bool bMagnetSimulating = MagnetMesh->IsSimulatingPhysics();
    const float StrengthTimesMultiplier = Strength * ForceMultiplier;

    for (UPrimitiveComponent* MetalComp : OverlappingMetals)
    {
        if (!IsValid(MetalComp) || !MetalComp->IsSimulatingPhysics())
        {
            continue;
        }

        const FVector MetalLoc = MetalComp->GetComponentLocation();
        const FVector ToMagnet = MagnetLoc - MetalLoc;
        const float Distance = ToMagnet.Size();

        if (Distance < MinDistance || Distance > MaxDistance)
        {
            continue;
        }

        const FVector Dir = ToMagnet / Distance;
        const float DirDot = FVector::DotProduct(Dir, MagnetForward);
        const float DirectionFactor = FMath::Lerp(0.75f, 1.0f, (DirDot + 1.0f) * 0.5f);

        float ForceMag = (StrengthTimesMultiplier * DirectionFactor) / FMath::Pow(Distance, MagneticDecayExponent);

        const float MetalMass = MetalComp->GetMass();
        ForceMag *= FMath::Clamp(MetalMass / 5.0f, 0.6f, 2.5f);

        const FVector CurrentVel = MetalComp->GetPhysicsLinearVelocity();
        const float VelTowardsMagnet = FVector::DotProduct(CurrentVel, Dir);

        float VelocityDamping = 1.0f;
        if (VelTowardsMagnet > MaxAttractVelocity * 0.7f)
        {
            VelocityDamping = FMath::Clamp(1.0f - (VelTowardsMagnet / MaxAttractVelocity), 0.4f, 1.0f);
        }

        const FVector DampingForce = -CurrentVel * (VelocityDampingFactor * MetalMass);
        FVector FinalForce = (Dir * ForceMag * VelocityDamping) + DampingForce;
        FinalForce = FinalForce.GetClampedToMaxSize(MaxForceClamp);

        MetalComp->AddForce(FinalForce, NAME_None, false);

        if (bUseTorque)
        {
            const FVector CrossProduct = FVector::CrossProduct(MetalComp->GetForwardVector(), Dir);
            const float TorqueMagnitude = CrossProduct.Size() * ForceMag * 0.3f;

            if (TorqueMagnitude > 0.01f)
            {
                MetalComp->AddTorqueInRadians(CrossProduct.GetSafeNormal() * TorqueMagnitude, NAME_None, false);
            }
        }

        if (bMagnetSimulating)
        {
            MagnetMesh->AddForce(-FinalForce * 0.2f, NAME_None, false);
        }

#if ENABLE_DRAW_DEBUG
        if (bDebugDraw)
        {
            DrawDebugLine(GetWorld(), MetalLoc, MagnetLoc, FColor::Cyan, false, -1.f, 0, 2.f);
            DrawDebugSphere(GetWorld(), MetalLoc, 25.f, 8, FColor::Red, false, -1.f);
            DrawDebugString(GetWorld(), MetalLoc + FVector(0, 0, 50),
                FString::Printf(TEXT("%.0f N"), FinalForce.Size()), nullptr, FColor::Yellow, 0.0f);
        }
#endif
    }

    if (bEnableInduction)
    {
        ApplyInducedMagnetism();
    }
}

void AMagnet::CheckDemagnetize()
{
    if (bDemagnetized)
    {
        return;
    }

    TArray<AActor*> HeatSources;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATemperature::StaticClass(), HeatSources);

    const FVector MyLoc = GetActorLocation();

    for (AActor* Actor : HeatSources)
    {
        const ATemperature* Heat = Cast<ATemperature>(Actor);
        if (!Heat)
        {
            continue;
        }

        const float DistCm = FVector::Dist(MyLoc, Heat->GetActorLocation());

        if (Heat->MaxHeatDistance > 0.f && DistCm <= Heat->MaxHeatDistance)
        {
            bDemagnetized = true;
            OverlappingMetals.Empty();

#if ENABLE_DRAW_DEBUG
            if (bDebugDraw)
            {
                DrawDebugString(GetWorld(), MyLoc + FVector(0, 0, 100),
                    TEXT("DEMAGNETIZED"), nullptr, FColor::Red, 5.0f, true);
            }
#endif
            return;
        }
    }
}

void AMagnet::RefreshOverlappingMetals()
{
    if (!MagnetRange)
    {
        return;
    }

    TArray<UPrimitiveComponent*> OverlappingComps;
    MagnetRange->GetOverlappingComponents(OverlappingComps);

    OverlappingMetals.Empty(OverlappingComps.Num());

    for (UPrimitiveComponent* Comp : OverlappingComps)
    {
        if (!Comp || !Comp->IsSimulatingPhysics())
        {
            continue;
        }

        const AActor* CompOwner = Comp->GetOwner();
        if (CompOwner && CompOwner != this && CompOwner->ActorHasTag(MetalTag))
        {
            OverlappingMetals.Add(Comp);
        }
    }
}

void AMagnet::OnRangeBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (bDemagnetized)
    {
        return;
    }

    if (!OtherActor || OtherActor == this || !OtherComp)
    {
        return;
    }

    if (OtherActor->ActorHasTag(MetalTag) && OtherComp->IsSimulatingPhysics())
    {
        OverlappingMetals.Add(OtherComp);

        if (bApplyInitialImpulse)
        {
            const FVector ToMagnet = (MagnetMesh->GetComponentLocation() - OtherComp->GetComponentLocation()).GetSafeNormal();
            OtherComp->AddImpulse(ToMagnet * InitialImpulseStrength * OtherComp->GetMass());
        }
    }
}

void AMagnet::OnRangeEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!OtherActor || !OtherComp)
    {
        return;
    }

    if (OtherActor->ActorHasTag(MetalTag))
    {
        OverlappingMetals.Remove(OtherComp);
    }
}

void AMagnet::ApplyInducedMagnetism()
{
    const FVector MagnetLoc = MagnetMesh->GetComponentLocation();
    const TArray<UPrimitiveComponent*> MetalArray = OverlappingMetals.Array();
    const int32 Num = MetalArray.Num();

    for (int32 i = 0; i < Num; ++i)
    {
        UPrimitiveComponent* MetalA = MetalArray[i];
        if (!IsValid(MetalA) || !MetalA->IsSimulatingPhysics())
        {
            continue;
        }

        const FVector MetalALoc = MetalA->GetComponentLocation();
        const float DistAToMagnet = FVector::Dist(MetalALoc, MagnetLoc);

        if (DistAToMagnet > MinDistanceForInduction)
        {
            continue;
        }

        const float InducedStrength = CalculateInducedStrength(DistAToMagnet, Strength);
        const FVector MagnetToA = (MetalALoc - MagnetLoc).GetSafeNormal();

        for (int32 j = i + 1; j < Num; ++j)
        {
            UPrimitiveComponent* MetalB = MetalArray[j];
            if (!IsValid(MetalB) || !MetalB->IsSimulatingPhysics())
            {
                continue;
            }

            const FVector MetalBLoc = MetalB->GetComponentLocation();
            const FVector AtoB = MetalBLoc - MetalALoc;
            const float DistAtoB = AtoB.Size();

            if (DistAtoB < 10.f || DistAtoB > InductionRange)
            {
                continue;
            }

            const FVector Dir = AtoB / DistAtoB;
            const float AlignmentFactor = FVector::DotProduct(Dir, MagnetToA);

            float ForceMag = (InducedStrength * InductionStrengthRatio * FMath::Abs(AlignmentFactor))
                           / FMath::Pow(DistAtoB, MagneticDecayExponent);

            const float MetalBMass = MetalB->GetMass();
            ForceMag *= FMath::Clamp(MetalBMass / 10.0f, 0.5f, 2.0f);

            const FVector CurrentVelB = MetalB->GetPhysicsLinearVelocity();
            const float VelTowardsA = FVector::DotProduct(CurrentVelB, Dir);

            float VelocityDamping = 1.0f;
            if (VelTowardsA > MaxAttractVelocity * 0.5f)
            {
                VelocityDamping = FMath::Clamp(1.0f - (VelTowardsA / MaxAttractVelocity), 0.3f, 1.0f);
            }

            const FVector DampingForce = -CurrentVelB * (VelocityDampingFactor * 0.5f * MetalBMass);
            FVector FinalForce = (Dir * ForceMag * VelocityDamping * AlignmentFactor) + DampingForce;
            FinalForce = FinalForce.GetClampedToMaxSize(MaxInducedForceClamp);

            MetalB->AddForce(FinalForce, NAME_None, false);
            MetalA->AddForce(-FinalForce * 0.5f, NAME_None, false);

#if ENABLE_DRAW_DEBUG
            if (bDebugDraw)
            {
                DrawDebugLine(GetWorld(), MetalALoc, MetalBLoc, FColor::Yellow, false, -1.f, 0, 1.f);
            }
#endif
        }
    }
}

float AMagnet::CalculateInducedStrength(float DistanceToMagnet, float BaseMagnetStrength) const
{
    const float SafeDist = FMath::Max(DistanceToMagnet, 1.0f);
    const float InductionFactor = FMath::Clamp(
        1.0f / FMath::Pow(SafeDist / MinDistanceForInduction, 1.5f), 0.0f, 1.0f);
    return BaseMagnetStrength * InductionFactor;
}