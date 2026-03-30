    #include "Magnet.h"
    #include "Wire.h"
    #include "Components/StaticMeshComponent.h"
    #include "Components/SphereComponent.h"
    #include "Components/SplineComponent.h"
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
        MagnetRange->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);

        WireContactRange = CreateDefaultSubobject<USphereComponent>(TEXT("WireContactRange"));
        WireContactRange->SetupAttachment(MagnetMesh);
        WireContactRange->SetSphereRadius(WireContactRadius);
        WireContactRange->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        WireContactRange->SetCollisionResponseToAllChannels(ECR_Overlap);

        static ConstructorHelpers::FClassFinder<AActor> ArrowBP(
        TEXT("/Game/modeling/Object/Arrow/Arrow_Effect"));
        if (ArrowBP.Succeeded())
        {
            ArrowEffectClass = ArrowBP.Class;
        }       
    }

    void AMagnet::BeginPlay()
    {
        Super::BeginPlay();

        if (bAutoComputeStrength)
        {
            Strength = MaxLiftMass * GravityAccel * FMath::Pow(ReferenceDistance, MagneticDecayExponent);
        }

        BaseStrength = Strength;

        MagnetRange->SetSphereRadius(MaxDistance);
        WireContactRange->SetSphereRadius(WireContactRadius);

        MagnetRange->OnComponentBeginOverlap.AddDynamic(this, &AMagnet::OnRangeBegin);
        MagnetRange->OnComponentEndOverlap.AddDynamic(this, &AMagnet::OnRangeEnd);
        WireContactRange->OnComponentBeginOverlap.AddDynamic(this, &AMagnet::OnWireContactBegin);
        WireContactRange->OnComponentEndOverlap.AddDynamic(this, &AMagnet::OnWireContactEnd);

        CheckDemagnetize();

        if (!bDemagnetized)
        {
            RefreshOverlappingMetals();
        }

if (bShowFieldArrows && !bDemagnetized && ArrowEffectClass)
{
    FTimerHandle SpawnTimerHandle;
    GetWorldTimerManager().SetTimer(SpawnTimerHandle, [this]()
    {
        if (!IsValid(this) || bDemagnetized || !ArrowEffectClass) return;

FQuat MagnetQuat = GetActorRotation().Quaternion();
FQuat OffsetQuat = FRotator(90.f, 0.f, 0.f).Quaternion();
FTransform SpawnTransform((MagnetQuat * OffsetQuat).Rotator(), GetActorLocation());

        AActor* Arrow = GetWorld()->SpawnActorDeferred<AActor>(
            ArrowEffectClass, SpawnTransform, this, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

        if (Arrow)
        {
            FProperty* PowerProp = Arrow->GetClass()->FindPropertyByName(TEXT("Power"));
            FProperty* XProp     = Arrow->GetClass()->FindPropertyByName(TEXT("X"));
            FProperty* YProp     = Arrow->GetClass()->FindPropertyByName(TEXT("Y"));

            if (PowerProp)
            {
                if (FDoubleProperty* D = CastField<FDoubleProperty>(PowerProp))
                    D->SetPropertyValue_InContainer(Arrow, (double)ArrowPower);
                else if (FFloatProperty* F = CastField<FFloatProperty>(PowerProp))
                    F->SetPropertyValue_InContainer(Arrow, ArrowPower);
            }
            if (XProp)
            {
                if (FDoubleProperty* D = CastField<FDoubleProperty>(XProp))
                    D->SetPropertyValue_InContainer(Arrow, (double)ArrowX);
                else if (FFloatProperty* F = CastField<FFloatProperty>(XProp))
                    F->SetPropertyValue_InContainer(Arrow, ArrowX);
            }
            if (YProp)
            {
                if (FDoubleProperty* D = CastField<FDoubleProperty>(YProp))
                    D->SetPropertyValue_InContainer(Arrow, (double)ArrowY);
                else if (FFloatProperty* F = CastField<FFloatProperty>(YProp))
                    F->SetPropertyValue_InContainer(Arrow, ArrowY);
            }

            UGameplayStatics::FinishSpawningActor(Arrow, SpawnTransform);

// 모든 컴포넌트를 Movable로 강제 설정
TArray<USceneComponent*> AllComps;
Arrow->GetRootComponent()->GetChildrenComponents(true, AllComps);
AllComps.Add(Arrow->GetRootComponent());
for (USceneComponent* Comp : AllComps)
{
    Comp->SetMobility(EComponentMobility::Movable);
}

SpawnedArrowEffect = Arrow;
        }
    }, 1.0f, false);
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

            UpdateElectroBoost();
            RefreshOverlappingMetals();
        }

    #if ENABLE_DRAW_DEBUG
        if (bDebugDraw)
        {
            if (bElectroActive)
            {
                DrawDebugSphere(GetWorld(), GetActorLocation(), WireContactRadius, 16, FColor::Cyan, false, -1.f, 0, 2.f);
                const float BoostRatio = (BaseStrength > 0.f) ? (Strength / BaseStrength) : 1.f;
                DrawDebugString(GetWorld(), GetActorLocation() + FVector(0, 0, 150),
                    FString::Printf(TEXT("[전자석 활성]\n연결 전선: %d개\n기본 자력: %.0f\n현재 자력: %.0f\n부스트: x%.2f"),
                        ContactedWires.Num(), BaseStrength, Strength, BoostRatio),
                    nullptr, FColor::Cyan, 0.0f, true);
            }
            else
            {
                DrawDebugString(GetWorld(), GetActorLocation() + FVector(0, 0, 150),
                    FString::Printf(TEXT("[일반 자석]\n자력: %.0f"), Strength),
                    nullptr, FColor::White, 0.0f, true);
            }
        }
    #endif

        if (OverlappingMetals.Num() == 0)
        {
            return;
        }

        const FVector MagnetLoc = MagnetMesh->GetComponentLocation();
        const FVector MagnetForward = MagnetMesh->GetForwardVector();
        const bool bMagnetSimulating = MagnetMesh->IsSimulatingPhysics();
        const float StrengthTimesMultiplier = Strength * ForceMultiplier;

        for (auto It = OverlappingMetals.CreateIterator(); It; ++It)
        {
            UPrimitiveComponent* Comp = It->Get();
            if (!IsValid(Comp))
            {
                It.RemoveCurrent();
                continue;
            }

            AActor* OwnerActor = Comp->GetOwner();
            if (!OwnerActor || !OwnerActor->ActorHasTag(MetalTag))
            {
                It.RemoveCurrent();
                continue;
            }
        }

        if (OverlappingMetals.Num() == 0)
        {
            return;
        }

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

            const float SafeDist = FMath::Max(Distance, MinDistance);
            float ForceMag = (StrengthTimesMultiplier * DirectionFactor) / FMath::Pow(SafeDist, MagneticDecayExponent);

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
                const FColor LineColor = bElectroActive ? FColor::Cyan : FColor::Blue;
                DrawDebugLine(GetWorld(), MetalLoc, MagnetLoc, LineColor, false, -1.f, 0, 2.f);
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

if (SpawnedArrowEffect)
{
    const FQuat OffsetQuat = FRotator(90.f, 0.f, 0.f).Quaternion();
    const FQuat DesiredQuat = GetActorQuat() * OffsetQuat;
    const FVector DesiredLoc = GetActorLocation();

    if (!SpawnedArrowEffect->GetActorQuat().Equals(DesiredQuat, 0.01f) ||
        !SpawnedArrowEffect->GetActorLocation().Equals(DesiredLoc, 1.f))
    {
        SpawnedArrowEffect->SetActorLocationAndRotation(DesiredLoc, DesiredQuat);
    }
}
    }

    void AMagnet::UpdateElectroBoost()
    {
        bool bAnyPowered = false;
        float TotalCurrent = 0.f;

        TArray<AActor*> NearbyActors;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWire::StaticClass(), NearbyActors);

        ContactedWires.Empty();

        const FVector MyLoc = GetActorLocation();

        for (AActor* Actor : NearbyActors)
        {
            AWire* Wire = Cast<AWire>(Actor);
            if (!Wire) continue;
            if (!Wire->IsPowered()) continue;

            bool bClose = false;

            USplineComponent* WireSpline = Wire->GetSplineComponent();
            if (WireSpline)
            {
                // 스플라인에서 가장 가까운 점 찾기
                const FVector Closest = WireSpline->FindLocationClosestToWorldLocation(MyLoc, ESplineCoordinateSpace::World);
                if (FVector::Dist(MyLoc, Closest) <= WireContactRadius)
                {
                    bClose = true;
                }

                // 포인트도 개별 체크
                if (!bClose)
                {
                    const int32 NumPoints = WireSpline->GetNumberOfSplinePoints();
                    for (int32 i = 0; i < NumPoints; ++i)
                    {
                        const FVector PointWorld = WireSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
                        if (FVector::Dist(MyLoc, PointWorld) <= WireContactRadius)
                        {
                            bClose = true;
                            break;
                        }
                    }
                }
            }
            else
            {
                bClose = FVector::Dist(MyLoc, Wire->GetActorLocation()) <= WireContactRadius;
            }

            if (bClose)
            {
                bAnyPowered = true;
                TotalCurrent += Wire->GetWireTemperature() / 100.f;
                ContactedWires.AddUnique(Wire);
            }
        }

        bElectroActive = bAnyPowered;

        if (bElectroActive)
        {
            const float CurrentBoost = FMath::Clamp(TotalCurrent, 1.0f, ElectroBoostMultiplier);
            Strength = BaseStrength * CurrentBoost;
        }
        else
        {
            Strength = BaseStrength;
        }
    }

    void AMagnet::OnWireContactBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult)
    {
        if (!OtherActor || OtherActor == this) return;

        if (AWire* Wire = Cast<AWire>(OtherActor))
        {
            ContactedWires.AddUnique(Wire);
            UpdateElectroBoost();
        }
    }

    void AMagnet::OnWireContactEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
    {
        if (!OtherActor) return;

        if (AWire* Wire = Cast<AWire>(OtherActor))
        {
            ContactedWires.Remove(Wire);
            UpdateElectroBoost();
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
                bElectroActive = false;
                Strength = 0.f;
                OverlappingMetals.Empty();
                ContactedWires.Empty();

                if (SpawnedArrowEffect)
                    {
                SpawnedArrowEffect->Destroy();
                 SpawnedArrowEffect = nullptr;
                    }       
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
        OverlappingMetals.Empty();

        UWorld* World = GetWorld();
        if (!World) return;

        const FVector Center = MagnetMesh->GetComponentLocation();
        FCollisionObjectQueryParams Obj = FCollisionObjectQueryParams::AllObjects;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(MagnetSense), false);
        Q.AddIgnoredActor(this);

        TArray<FOverlapResult> Hits;
        World->OverlapMultiByObjectType(Hits, Center, FQuat::Identity, Obj,
            FCollisionShape::MakeSphere(MaxDistance), Q);

        for (const FOverlapResult& H : Hits)
        {
            UPrimitiveComponent* Comp = H.GetComponent();
            if (!Comp) continue;

            AActor* CompOwner = Comp->GetOwner();
            if (!CompOwner || CompOwner == this) continue;

            if (CompOwner->ActorHasTag(MetalTag) && Comp->IsSimulatingPhysics())
            {
                OverlappingMetals.Add(Comp);
            }
        }
    }

    void AMagnet::OnRangeBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
    {
        if (bDemagnetized) return;
        if (!OtherActor || OtherActor == this || !OtherComp) return;

        if (OtherActor->ActorHasTag(MetalTag) && OtherComp->IsSimulatingPhysics())
        {
            const bool bAlreadyInside = OverlappingMetals.Contains(OtherComp);

            if (!bAlreadyInside)
            {
                OverlappingMetals.Add(OtherComp);

                if (bApplyInitialImpulse)
                {
                    const FVector ToMagnet =
                        (MagnetMesh->GetComponentLocation() - OtherComp->GetComponentLocation()).GetSafeNormal();
                    OtherComp->AddImpulse(ToMagnet * InitialImpulseStrength * OtherComp->GetMass());
                }
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
