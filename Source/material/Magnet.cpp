#include "Magnet.h"
#include "Wire.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SplineComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Temperature.h"
#include "EngineUtils.h"
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
    UE_LOG(LogTemp, Warning, TEXT("[Magnet] BeginPlay 진입! bShowFieldArrows=%d"), bShowFieldArrows ? 1 : 0);

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
        SpawnArrowEffect();
    }
}

void AMagnet::SpawnArrowEffect()
{
    UE_LOG(LogTemp, Warning, TEXT("[Magnet] SpawnArrowEffect 호출! Arrow=%d"), SpawnedArrowEffect ? 1 : 0);

    if (!IsValid(this) || bDemagnetized || !ArrowEffectClass) return;

    const FQuat MagnetQuat = GetActorRotation().Quaternion();
    const FQuat OffsetQuat = FRotator(90.f, 0.f, 0.f).Quaternion();
    FTransform SpawnTransform((MagnetQuat * OffsetQuat).Rotator(), GetActorLocation());

    AActor* Arrow = GetWorld()->SpawnActorDeferred<AActor>(
        ArrowEffectClass, SpawnTransform, this, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    UE_LOG(LogTemp, Warning, TEXT("[Magnet] SpawnActorDeferred 결과: Arrow=%d, Class=%s"),
        Arrow ? 1 : 0,
        ArrowEffectClass ? *ArrowEffectClass->GetName() : TEXT("NULL"));

    if (!Arrow) return;

    auto SetFloatProp = [Arrow](const TCHAR* PropName, float Value)
    {
        if (FProperty* Prop = Arrow->GetClass()->FindPropertyByName(PropName))
        {
            if (FDoubleProperty* D = CastField<FDoubleProperty>(Prop))
                D->SetPropertyValue_InContainer(Arrow, (double)Value);
            else if (FFloatProperty* F = CastField<FFloatProperty>(Prop))
                F->SetPropertyValue_InContainer(Arrow, Value);
        }
    };

    SetFloatProp(TEXT("Power"), ArrowPower);
    SetFloatProp(TEXT("X"),     ArrowX);
    SetFloatProp(TEXT("Y"),     ArrowY);

    UGameplayStatics::FinishSpawningActor(Arrow, SpawnTransform);

    TArray<USceneComponent*> AllComps;
    Arrow->GetRootComponent()->GetChildrenComponents(true, AllComps);
    AllComps.Add(Arrow->GetRootComponent());

    for (USceneComponent* Comp : AllComps)
    {
        Comp->SetMobility(EComponentMobility::Movable);

        if (UPrimitiveComponent* PrimC = Cast<UPrimitiveComponent>(Comp))
        {
            PrimC->SetRenderCustomDepth(true);
            PrimC->SetCustomDepthStencilValue(255);
            PrimC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }

    SpawnedArrowEffect = Arrow;

    if (SpawnedArrowEffect && MagnetMesh)
    {
        SpawnedArrowEffect->AttachToComponent(
            MagnetMesh, FAttachmentTransformRules::KeepRelativeTransform);

        FVector MinBounds, MaxBounds;
        MagnetMesh->GetLocalBounds(MinBounds, MaxBounds);

        const float HalfHeight = (MaxBounds.Z - MinBounds.Z) * 0.5f;
        const float PosZ = -HalfHeight * ArrowDownRatio;
        SpawnedArrowEffect->SetActorRelativeLocation(FVector(0.f, 0.f, PosZ));
        SpawnedArrowEffect->SetActorRelativeRotation(FRotator(90.f, 0.f, 0.f));

        const float ScaleX = (MaxBounds.X / 50.f) * ArrowScaleMul;
        const float ScaleY = (MaxBounds.Y / 50.f) * ArrowScaleMul;
        const float ScaleZ = (MaxBounds.Z / 50.f) * ArrowScaleMul;
        SpawnedArrowEffect->SetActorRelativeScale3D(FVector(ScaleX, ScaleY, ScaleZ));
    }

    RefreshArrowVisibility();
}

void AMagnet::SyncArrowTransform()
{
    if (!SpawnedArrowEffect) return;

    const FQuat OffsetQuat  = FRotator(90.f, 0.f, 0.f).Quaternion();
    const FQuat DesiredQuat = GetActorQuat() * OffsetQuat;
    const FVector DesiredLoc = GetActorLocation();

    if (!SpawnedArrowEffect->GetActorQuat().Equals(DesiredQuat, 0.01f) ||
        !SpawnedArrowEffect->GetActorLocation().Equals(DesiredLoc, 1.f))
    {
        SpawnedArrowEffect->SetActorLocationAndRotation(DesiredLoc, DesiredQuat);
    }
}

void AMagnet::UpdateArrowVisibility()
{
    if (!SpawnedArrowEffect) return;
    SpawnedArrowEffect->SetActorHiddenInGame(bDemagnetized);
}

void AMagnet::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (RotationSpeed > 0.f)
    {
        const float RotDir = bRotateClockwise ? 1.f : -1.f;
        FRotator CurrentRot = GetActorRotation();
        CurrentRot.Yaw += RotDir * RotationSpeed * DeltaTime;
        SetActorRotation(CurrentRot);
    }

    if (bDemagnetized)
    {
        UpdateArrowVisibility();
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
            UpdateArrowVisibility();
            return;
        }

        UpdateElectroBoost();
        RefreshOverlappingMetals();
    }

#if ENABLE_DRAW_DEBUG
    if (bDebugDraw)
    {
        const FVector NorthDir = GetNorthPoleWorldDir();
        DrawDebugDirectionalArrow(GetWorld(),
            GetActorLocation(),
            GetActorLocation() + NorthDir * 80.f,
            20.f, FColor::Red, false, -1.f, 0, 3.f);

        DrawDebugString(GetWorld(), GetActorLocation() + FVector(0, 0, 80.f),
            FString::Printf(TEXT("[%s극]\n자력: %.0f"),
                bIsNorthPole ? TEXT("N") : TEXT("S"), Strength),
            nullptr, bIsNorthPole ? FColor::Red : FColor::Blue, 0.f, true);

        if (bElectroActive)
        {
            DrawDebugSphere(GetWorld(), GetActorLocation(),
                WireContactRadius, 16, FColor::Cyan, false, -1.f, 0, 2.f);
        }
    }
#endif

    if (OverlappingMetals.Num() == 0)
    {
        return;
    }

    const FVector MagnetLoc               = MagnetMesh->GetComponentLocation();
    const FVector MagnetForward           = MagnetMesh->GetForwardVector();
    const bool    bMagnetSimulating       = MagnetMesh->IsSimulatingPhysics();
    const float   StrengthTimesMultiplier = Strength * ForceMultiplier;

    for (auto It = OverlappingMetals.CreateIterator(); It; ++It)
    {
        UPrimitiveComponent* Comp = It->Get();
        if (!IsValid(Comp)) { It.RemoveCurrent(); continue; }

        AActor* OwnerActor = Comp->GetOwner();
        if (!OwnerActor || !OwnerActor->ActorHasTag(MetalTag)) { It.RemoveCurrent(); }
    }

    for (UPrimitiveComponent* MetalComp : OverlappingMetals)
    {
        if (!IsValid(MetalComp) || !MetalComp->IsSimulatingPhysics()) continue;

        const FVector MetalLoc = MetalComp->GetComponentLocation();
        const FVector ToMagnet = MagnetLoc - MetalLoc;
        const float   Distance = ToMagnet.Size();

        if (Distance < MinDistance || Distance > MaxDistance) continue;

        const FVector Dir       = ToMagnet / Distance;
        const float   DirDot    = FVector::DotProduct(Dir, MagnetForward);
        const float   DirFactor = FMath::Lerp(0.75f, 1.0f, (DirDot + 1.0f) * 0.5f);
        const float   SafeDist  = FMath::Max(Distance, MinDistance);

        float ForceMag = (StrengthTimesMultiplier * DirFactor)
                       / FMath::Pow(SafeDist, MagneticDecayExponent);

        const float MetalMass = MetalComp->GetMass();
        ForceMag *= FMath::Clamp(MetalMass / 5.0f, 0.6f, 2.5f);

        const FVector CurVel           = MetalComp->GetPhysicsLinearVelocity();
        const float   VelTowardsMagnet = FVector::DotProduct(CurVel, Dir);

        float VelocityDamping = 1.0f;
        if (VelTowardsMagnet > MaxAttractVelocity * 0.7f)
        {
            VelocityDamping = FMath::Clamp(
                1.0f - (VelTowardsMagnet / MaxAttractVelocity), 0.4f, 1.0f);
        }

        const FVector DampingForce = -CurVel * (VelocityDampingFactor * MetalMass);
        FVector FinalForce = (Dir * ForceMag * VelocityDamping) + DampingForce;
        FinalForce = FinalForce.GetClampedToMaxSize(MaxForceClamp);

        MetalComp->AddForce(FinalForce, NAME_None, false);

        if (bUseTorque)
        {
            const FVector Cross    = FVector::CrossProduct(MetalComp->GetForwardVector(), Dir);
            const float   TorqueMag = Cross.Size() * ForceMag * 0.3f;
            if (TorqueMag > 0.01f)
                MetalComp->AddTorqueInRadians(Cross.GetSafeNormal() * TorqueMag, NAME_None, false);
        }

        if (bMagnetSimulating)
            MagnetMesh->AddForce(-FinalForce * 0.2f, NAME_None, false);

#if ENABLE_DRAW_DEBUG
        if (bDebugDraw)
        {
            const FColor LineColor = bElectroActive ? FColor::Cyan : FColor::Blue;
            DrawDebugLine(GetWorld(), MetalLoc, MagnetLoc, LineColor, false, -1.f, 0, 2.f);
            DrawDebugSphere(GetWorld(), MetalLoc, 25.f, 8, FColor::Red, false, -1.f);
            DrawDebugString(GetWorld(), MetalLoc + FVector(0, 0, 50),
                FString::Printf(TEXT("%.0f N"), FinalForce.Size()),
                nullptr, FColor::Yellow, 0.f);
        }
#endif
    }

    if (bEnableInduction)
        ApplyInducedMagnetism();
}

FVector AMagnet::GetNorthPoleWorldDir() const
{
    if (!MagnetMesh) return FVector::ForwardVector;
    return MagnetMesh->GetComponentTransform()
        .TransformVectorNoScale(NorthPoleLocalDir)
        .GetSafeNormal();
}

FVector AMagnet::GetSouthPoleWorldDir() const
{
    return -GetNorthPoleWorldDir();
}

void AMagnet::UpdateElectroBoost()
{
    bool  bAnyPowered  = false;
    float TotalCurrent = 0.f;

    TArray<AActor*> NearbyActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWire::StaticClass(), NearbyActors);

    ContactedWires.Empty();
    const FVector MyLoc = GetActorLocation();

    for (AActor* Actor : NearbyActors)
    {
        AWire* Wire = Cast<AWire>(Actor);
        if (!Wire || !Wire->IsPowered()) continue;

        bool bClose = false;

        if (USplineComponent* WireSpline = Wire->GetSplineComponent())
        {
            const FVector Closest = WireSpline->FindLocationClosestToWorldLocation(
                MyLoc, ESplineCoordinateSpace::World);
            bClose = FVector::Dist(MyLoc, Closest) <= WireContactRadius;

            if (!bClose)
            {
                const int32 NumPoints = WireSpline->GetNumberOfSplinePoints();
                for (int32 i = 0; i < NumPoints; ++i)
                {
                    if (FVector::Dist(MyLoc,
                        WireSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World))
                        <= WireContactRadius)
                    { bClose = true; break; }
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
    Strength = bElectroActive
        ? BaseStrength * FMath::Clamp(TotalCurrent, 1.0f, ElectroBoostMultiplier)
        : BaseStrength;
}

void AMagnet::OnWireContactBegin(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!OtherActor || OtherActor == this) return;
    if (AWire* Wire = Cast<AWire>(OtherActor))
    {
        ContactedWires.AddUnique(Wire);
        UpdateElectroBoost();
    }
}

void AMagnet::OnWireContactEnd(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32)
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
    if (bDemagnetized) return;

    TArray<AActor*> HeatSources;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATemperature::StaticClass(), HeatSources);

    const FVector MyLoc = GetActorLocation();

    for (AActor* Actor : HeatSources)
    {
        const ATemperature* Heat = Cast<ATemperature>(Actor);
        if (!Heat) continue;

        const float DistCm = FVector::Dist(MyLoc, Heat->GetActorLocation());
        if (Heat->MaxHeatDistance > 0.f && DistCm <= Heat->MaxHeatDistance)
        {
            bDemagnetized  = true;
            bElectroActive = false;
            Strength       = 0.f;
            OverlappingMetals.Empty();
            ContactedWires.Empty();

            if (SpawnedArrowEffect)
                SpawnedArrowEffect->SetActorHiddenInGame(true);

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
    FCollisionQueryParams Q(SCENE_QUERY_STAT(MagnetSense), false);
    Q.AddIgnoredActor(this);

    TArray<FOverlapResult> Hits;
    World->OverlapMultiByObjectType(Hits, Center, FQuat::Identity,
        FCollisionObjectQueryParams::AllObjects,
        FCollisionShape::MakeSphere(MaxDistance), Q);

    for (const FOverlapResult& H : Hits)
    {
        UPrimitiveComponent* Comp = H.GetComponent();
        if (!Comp) continue;

        AActor* CompOwner = Comp->GetOwner();
        if (!CompOwner || CompOwner == this) continue;

        if (CompOwner->ActorHasTag(MetalTag) && Comp->IsSimulatingPhysics())
            OverlappingMetals.Add(Comp);
    }
}

void AMagnet::OnRangeBegin(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32, bool, const FHitResult&)
{
    if (bDemagnetized || !OtherActor || OtherActor == this || !OtherComp) return;

    if (OtherActor->ActorHasTag(MetalTag) && OtherComp->IsSimulatingPhysics())
    {
        if (!OverlappingMetals.Contains(OtherComp))
        {
            OverlappingMetals.Add(OtherComp);

            if (bApplyInitialImpulse)
            {
                const FVector ToMagnet =
                    (MagnetMesh->GetComponentLocation() - OtherComp->GetComponentLocation())
                    .GetSafeNormal();
                OtherComp->AddImpulse(ToMagnet * InitialImpulseStrength * OtherComp->GetMass());
            }
        }
    }
}

void AMagnet::OnRangeEnd(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32)
{
    if (!OtherActor || !OtherComp) return;
    if (OtherActor->ActorHasTag(MetalTag))
        OverlappingMetals.Remove(OtherComp);
}

void AMagnet::ApplyInducedMagnetism()
{
    const FVector MagnetLoc = MagnetMesh->GetComponentLocation();
    const TArray<UPrimitiveComponent*> MetalArray = OverlappingMetals.Array();
    const int32 Num = MetalArray.Num();

    for (int32 i = 0; i < Num; ++i)
    {
        UPrimitiveComponent* MetalA = MetalArray[i];
        if (!IsValid(MetalA) || !MetalA->IsSimulatingPhysics()) continue;

        const FVector MetalALoc     = MetalA->GetComponentLocation();
        const float   DistAToMagnet = FVector::Dist(MetalALoc, MagnetLoc);
        if (DistAToMagnet > MinDistanceForInduction) continue;

        const float   InducedStr = CalculateInducedStrength(DistAToMagnet, Strength);
        const FVector MagnetToA  = (MetalALoc - MagnetLoc).GetSafeNormal();

        for (int32 j = i + 1; j < Num; ++j)
        {
            UPrimitiveComponent* MetalB = MetalArray[j];
            if (!IsValid(MetalB) || !MetalB->IsSimulatingPhysics()) continue;

            const FVector AtoB     = MetalB->GetComponentLocation() - MetalALoc;
            const float   DistAtoB = AtoB.Size();
            if (DistAtoB < 10.f || DistAtoB > InductionRange) continue;

            const FVector Dir       = AtoB / DistAtoB;
            const float   Alignment = FVector::DotProduct(Dir, MagnetToA);

            float ForceMag = (InducedStr * InductionStrengthRatio * FMath::Abs(Alignment))
                           / FMath::Pow(DistAtoB, MagneticDecayExponent);
            ForceMag *= FMath::Clamp(MetalB->GetMass() / 10.0f, 0.5f, 2.0f);

            const FVector VelB   = MetalB->GetPhysicsLinearVelocity();
            const float   VelToA = FVector::DotProduct(VelB, Dir);
            float VelDamp = 1.0f;
            if (VelToA > MaxAttractVelocity * 0.5f)
                VelDamp = FMath::Clamp(1.0f - (VelToA / MaxAttractVelocity), 0.3f, 1.0f);

            FVector Force = (Dir * ForceMag * VelDamp * Alignment)
                          + (-VelB * VelocityDampingFactor * 0.5f * MetalB->GetMass());
            Force = Force.GetClampedToMaxSize(MaxInducedForceClamp);

            MetalB->AddForce(Force, NAME_None, false);
            MetalA->AddForce(-Force * 0.5f, NAME_None, false);
        }
    }
}

float AMagnet::CalculateInducedStrength(float DistanceToMagnet, float BaseMagnetStrength) const
{
    const float SafeDist = FMath::Max(DistanceToMagnet, 1.0f);
    return BaseMagnetStrength * FMath::Clamp(
        1.0f / FMath::Pow(SafeDist / MinDistanceForInduction, 1.5f), 0.0f, 1.0f);
}

void AMagnet::SetAllArrowsVisible(bool bVisible)
{
    for (TObjectIterator<AMagnet> It; It; ++It)
    {
        AMagnet* Magnet = *It;
        if (!IsValid(Magnet) || !Magnet->SpawnedArrowEffect) continue;

        Magnet->SpawnedArrowEffect->SetActorHiddenInGame(!bVisible);

        TArray<UActorComponent*> MeshComps;
        Magnet->SpawnedArrowEffect->GetComponents(UMeshComponent::StaticClass(), MeshComps);
        for (UActorComponent* Comp : MeshComps)
        {
            if (UMeshComponent* Mesh = Cast<UMeshComponent>(Comp))
            {
                Mesh->SetCustomDepthStencilValue(bVisible ? 255 : 0);
                Mesh->SetRenderCustomDepth(bVisible);
            }
        }
    }
}

void AMagnet::SetGlobalMagnetCameraState(const UObject* WorldContextObject, bool bIsCameraOn)
{
    if (!WorldContextObject || !WorldContextObject->GetWorld())
        return;

    TArray<AActor*> FoundMagnets;
    UGameplayStatics::GetAllActorsOfClass(
        WorldContextObject->GetWorld(), AMagnet::StaticClass(), FoundMagnets);

    for (AActor* Actor : FoundMagnets)
    {
        if (AMagnet* Mag = Cast<AMagnet>(Actor))
            Mag->SetMagneticCameraState(bIsCameraOn);
    }
}

void AMagnet::SetMagneticCameraState(bool bIsCameraOn)
{
    bIsMagneticCameraOn = bIsCameraOn;
    UE_LOG(LogTemp, Warning, TEXT("[Magnet] CameraState=%d, Arrow=%d"),
        bIsCameraOn ? 1 : 0, SpawnedArrowEffect ? 1 : 0);
    RefreshArrowVisibility();
}

void AMagnet::RefreshArrowVisibility()
{
    if (!SpawnedArrowEffect) return;

    const bool bShouldShow = bIsMagneticCameraOn && !bDemagnetized;
    SpawnedArrowEffect->SetActorHiddenInGame(!bShouldShow);
}

// ★ 발판용 강제 소자
void AMagnet::ForceDemagnetize()
{
    if (bDemagnetized) return;

    bDemagnetized  = true;
    bElectroActive = false;
    Strength       = 0.f;
    OverlappingMetals.Empty();
    ContactedWires.Empty();

    // Arrow 숨김
    if (SpawnedArrowEffect)
        SpawnedArrowEffect->SetActorHiddenInGame(true);
}

// ★ 발판용 완전 복구
void AMagnet::Restore()
{
    if (!bDemagnetized) return;

    bDemagnetized = false;
    Strength      = BaseStrength;

    // 자석 메시 복구
    if (MagnetMesh)
        MagnetMesh->SetVisibility(true);

    // Arrow 복구 (카메라 상태 반영)
    RefreshArrowVisibility();

    // 금속 오버랩 다시 감지
    RefreshOverlappingMetals();
}