#include "Coil.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Wire.h"
#include "InductionPlate.h"
#include "UObject/ConstructorHelpers.h"

#include "Engine/OverlapResult.h"

ACoil::ACoil()
{
	PrimaryActorTick.bCanEverTick = true;

	CoilMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoilMesh"));
	SetRootComponent(CoilMesh);
	CoilMesh->SetMobility(EComponentMobility::Movable);
	CoilMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	CoilMesh->SetGenerateOverlapEvents(true);

	DetectionZone = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionZone"));
	DetectionZone->SetupAttachment(RootComponent);
	DetectionZone->SetBoxExtent(DetectionBoxExtent);
	DetectionZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectionZone->SetCollisionResponseToAllChannels(ECR_Overlap);
	DetectionZone->SetGenerateOverlapEvents(true);
	DetectionZone->bDrawOnlyIfSelected = !bShowDebugShapes;
	DetectionZone->ShapeColor = FColor::Cyan;
	DetectionZone->SetHiddenInGame(!bShowDebugShapes);
	DetectionZone->SetVisibility(bShowDebugShapes);

	MagneticFieldSphere = CreateDefaultSubobject<USphereComponent>(TEXT("MagneticFieldSphere"));
	MagneticFieldSphere->SetupAttachment(RootComponent);
	MagneticFieldSphere->SetSphereRadius(MagneticFieldRadius);
	MagneticFieldSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MagneticFieldSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
	MagneticFieldSphere->SetGenerateOverlapEvents(true);
	MagneticFieldSphere->bDrawOnlyIfSelected = !bShowDebugShapes;
	MagneticFieldSphere->ShapeColor = FColor::Blue;
	MagneticFieldSphere->SetHiddenInGame(!bShowDebugShapes);
	MagneticFieldSphere->SetVisibility(bShowDebugShapes);

	BottomBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("BottomBlocker"));
	BottomBlocker->SetupAttachment(RootComponent);
	BottomBlocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BottomBlocker->SetCollisionResponseToAllChannels(ECR_Block);
	BottomBlocker->bDrawOnlyIfSelected = true;   // 에디터에서 선택 안 하면 와이어프레임 안 보임
	BottomBlocker->ShapeColor = FColor::Red;
	BottomBlocker->SetVisibility(true);
	BottomBlocker->SetHiddenInGame(true);        // 인게임에서는 와이어프레임 안 보임 (대신 BottomPlateMesh가 보임)

	// 인게임에서 실제로 보이는 판 메시 (BottomBlocker 위치에 부착)
	BottomPlateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BottomPlateMesh"));
	BottomPlateMesh->SetupAttachment(BottomBlocker);
	BottomPlateMesh->SetRelativeLocation(FVector::ZeroVector);
	BottomPlateMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 충돌은 BottomBlocker가 담당

	// 기본 큐브 메시 자동 로드
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		BottomPlateMesh->SetStaticMesh(CubeMeshFinder.Object);
	}
}

void ACoil::BeginPlay()
{
	Super::BeginPlay();

	DetectionZone->SetBoxExtent(DetectionBoxExtent);
	MagneticFieldSphere->SetSphereRadius(MagneticFieldRadius);
	BaseCoilLocation = GetActorLocation();

	// 부착 액터들의 상대 오프셋 기록
	AttachedOffsets.Empty();
	for (AActor* A : AttachedActors)
	{
		if (A)
			AttachedOffsets.Add(A->GetActorLocation() - BaseCoilLocation);
		else
			AttachedOffsets.Add(FVector::ZeroVector);
	}

	CoilMesh->SetSimulatePhysics(false);

	ApplyDebugVisibility();

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		EnableInput(PC);
		if (InputComponent)
		{
			InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ACoil::ToggleCoil);
		}
	}
}

void ACoil::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bCoilActive)
	{
		for (const TWeakObjectPtr<AActor>& MagnetPtr : DetectedMagnets)
		{
			if (AActor* Magnet = MagnetPtr.Get())
			{
				if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Magnet->GetRootComponent()))
				{
					if (Root->IsSimulatingPhysics())
					{
						Root->SetPhysicsLinearVelocity(FVector::ZeroVector);
						Root->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
					}
				}
			}
		}

		DetectedMagnets.Empty();
		CurrentEMF = 0.f;
		OscillationTime = 0.f;
		SetActorLocation(BaseCoilLocation, false);

		// ★ 꺼진 동안 전선이 켜져있으면 정리 (안전망)
		for (const TWeakObjectPtr<AActor>& WirePtr : ConnectedWires)
		{
			if (AWire* W = Cast<AWire>(WirePtr.Get()))
			{
				W->SetBatterySource(false);
				W->SetPowered(false);
			}
		}
		ConnectedWires.Empty();

		DebugVisualize();
		return;
	}

	DetectMagnets();
	ApplyOscillation(DeltaTime);
	UpdateFieldRadius();
	ApplyMagneticForce();
	DebugVisualize();
	UpdateCircuit();
}

void ACoil::ToggleCoil()
{
	SetCoilActive(!bCoilActive);
}

void ACoil::SetCoilActive(bool bNewActive)
{
	if (bCoilActive == bNewActive) return;
	bCoilActive = bNewActive;
	UE_LOG(LogTemp, Log, TEXT("Coil [%s] -> %s"), *GetName(), bCoilActive ? TEXT("ON") : TEXT("OFF"));

	// 꺼질 때 전선 즉시 정리
	if (!bCoilActive)
	{
		for (const TWeakObjectPtr<AActor>& WirePtr : ConnectedWires)
		{
			if (AWire* W = Cast<AWire>(WirePtr.Get()))
			{
				W->SetBatterySource(false);
				W->SetPowered(false);
			}
		}
		ConnectedWires.Empty();
		CurrentEMF = 0.f;
	}
}

void ACoil::DetectMagnets()
{
	DetectedMagnets.Empty();
	if (!DetectionZone) return;

	TArray<FOverlapResult> Hits;
	FCollisionQueryParams QParams(SCENE_QUERY_STAT(CoilDetect), false);
	QParams.AddIgnoredActor(this);

	GetWorld()->OverlapMultiByObjectType(
		Hits,
		BaseCoilLocation,
		DetectionZone->GetComponentQuat(),
		FCollisionObjectQueryParams::AllObjects,
		FCollisionShape::MakeBox(DetectionBoxExtent),
		QParams
	);

	for (const FOverlapResult& H : Hits)
	{
		AActor* HitActor = H.GetActor();
		if (!HitActor || HitActor == this) continue;
		if (!HitActor->ActorHasTag(MagnetTag)) continue;

		bool bAlreadyAdded = false;
		for (const TWeakObjectPtr<AActor>& Existing : DetectedMagnets)
		{
			if (Existing.Get() == HitActor) { bAlreadyAdded = true; break; }
		}
		if (!bAlreadyAdded)
		{
			DetectedMagnets.Add(HitActor);
		}
	}

	CurrentEMF = 0.f;
	if (bCoilActive && HasMagnetInside())
	{
		const int32 Count = DetectedMagnets.Num();
		const float ScaledSpeed = OscillationSpeed + (Count - 1) * SpeedPerExtraMagnet;
		float TotalB = MagnetFieldStrengthTesla * Count;
		float VelocityFactor = FMath::Abs(FMath::Cos(OscillationTime * ScaledSpeed));

		float RadiusMeters = (CoilInnerDiameterCM / 100.f) / 2.f;
		float Area = PI * RadiusMeters * RadiusMeters;

		CurrentEMF = CoilWindings * TotalB * Area * ScaledSpeed * VelocityFactor;
	}
}

void ACoil::ApplyOscillation(float DeltaTime)
{
	if (HasMagnetInside())
	{
		OscillationTime += DeltaTime;

		const int32 Count = DetectedMagnets.Num();
		const float ScaledAmplitude = OscillationAmplitude * Count;
		const float ScaledSpeed = OscillationSpeed + (Count - 1) * SpeedPerExtraMagnet;

		const float OffsetZ = FMath::Sin(OscillationTime * ScaledSpeed) * ScaledAmplitude;
		const FVector NewLoc = BaseCoilLocation + FVector(0.f, 0.f, OffsetZ);
		SetActorLocation(NewLoc, false);

		MagneticFieldSphere->SetWorldLocation(BaseCoilLocation);

		// ★ 부착 액터들도 같은 위치 관계 유지하며 이동
		for (int32 i = 0; i < AttachedActors.Num(); ++i)
		{
			if (AttachedActors[i] && AttachedOffsets.IsValidIndex(i))
				AttachedActors[i]->SetActorLocation(NewLoc + AttachedOffsets[i], false);
		}
	}
	else
	{
		OscillationTime = 0.f;
		SetActorLocation(BaseCoilLocation, false);

		// ★ 멈출 때도 원위치로
		for (int32 i = 0; i < AttachedActors.Num(); ++i)
		{
			if (AttachedActors[i] && AttachedOffsets.IsValidIndex(i))
				AttachedActors[i]->SetActorLocation(BaseCoilLocation + AttachedOffsets[i], false);
		}
	}
}

void ACoil::UpdateFieldRadius()
{
	const float DynamicRadius = HasMagnetInside()
		? MagneticFieldRadius + (DetectedMagnets.Num() - 1) * FieldRadiusPerMagnet
		: MagneticFieldRadius;

	MagneticFieldSphere->SetSphereRadius(DynamicRadius);
}

void ACoil::ApplyMagneticForce()
{
	if (!HasMagnetInside()) return;

	for (const TWeakObjectPtr<AActor>& MagnetPtr : DetectedMagnets)
	{
		AActor* Magnet = MagnetPtr.Get();
		if (!Magnet) continue;

		UPrimitiveComponent* MagnetRoot = Cast<UPrimitiveComponent>(Magnet->GetRootComponent());
		if (!MagnetRoot || !MagnetRoot->IsSimulatingPhysics()) continue;

		const FVector Direction = BaseCoilLocation - Magnet->GetActorLocation();
		const float Distance = Direction.Size();
		if (Distance < 1.f) continue;

		const float ForceMag = MagneticForceStrength / (DetectedMagnets.Num() * FMath::Max(Distance * Distance, 100.f));
		const FVector Force = Direction.GetSafeNormal() * ForceMag;

		MagnetRoot->AddForce(Force, NAME_None, true);
	}
}

void ACoil::DebugVisualize()
{
#if ENABLE_DRAW_DEBUG
	if (!bDebugDraw) return;

	const bool bActive = bCoilActive && HasMagnetInside();

	if (!bCoilActive)
	{
		DrawDebugBox(GetWorld(), BaseCoilLocation,
			DetectionBoxExtent, DetectionZone->GetComponentQuat(),
			FColor(40, 40, 40), false, 0.f, 0, 2.f);

		DrawDebugString(GetWorld(), BaseCoilLocation + FVector(0.f, 0.f, 60.f),
			TEXT("Coil OFF (Press F)"), nullptr, FColor::Silver, 0.f, true);
		return;
	}

	const FColor BoxColor = bActive ? FColor::Green : FColor::Red;
	DrawDebugBox(GetWorld(), BaseCoilLocation,
		DetectionBoxExtent, DetectionZone->GetComponentQuat(),
		BoxColor, false, 0.f, 0, 2.f);

	const float CurrentRadius = MagneticFieldSphere->GetScaledSphereRadius();
	const FColor SphereColor = bActive ? FColor::Blue : FColor(80, 80, 80);
	DrawDebugSphere(GetWorld(), BaseCoilLocation,
		CurrentRadius, 24, SphereColor, false, 0.f, 0, 1.f);

	if (bActive)
	{
		DrawDebugString(GetWorld(), BaseCoilLocation + FVector(0.f, 0.f, 60.f),
			FString::Printf(TEXT("Magnets: %d | EMF: %.2f V | N: %d"),
				DetectedMagnets.Num(), CurrentEMF, CoilWindings),
			nullptr, FColor::Green, 0.f, true);
	}

	for (const TWeakObjectPtr<AActor>& MagnetPtr : DetectedMagnets)
	{
		if (AActor* M = MagnetPtr.Get())
		{
			DrawDebugDirectionalArrow(GetWorld(),
				M->GetActorLocation(), BaseCoilLocation,
				20.f, FColor::Yellow, false, 0.f, 0, 2.f);
		}
	}
#endif
}

void ACoil::ApplyDebugVisibility()
{
	// DetectionZone 가시성
	if (DetectionZone)
	{
		DetectionZone->SetHiddenInGame(!bShowDebugShapes);
		DetectionZone->SetVisibility(bShowDebugShapes);
		DetectionZone->bDrawOnlyIfSelected = !bShowDebugShapes;
		DetectionZone->MarkRenderStateDirty();
	}

	// MagneticFieldSphere 가시성
	if (MagneticFieldSphere)
	{
		MagneticFieldSphere->SetHiddenInGame(!bShowDebugShapes);
		MagneticFieldSphere->SetVisibility(bShowDebugShapes);
		MagneticFieldSphere->bDrawOnlyIfSelected = !bShowDebugShapes;
		MagneticFieldSphere->MarkRenderStateDirty();
	}

	// BottomBlocker는 항상 보이므로 여기서 제어하지 않음
}

#if WITH_EDITOR
void ACoil::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName = (PropertyChangedEvent.Property != nullptr)
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (PropName == GET_MEMBER_NAME_CHECKED(ACoil, bShowDebugShapes))
	{
		ApplyDebugVisibility();
	}
}
#endif

void ACoil::UpdateCircuit()
{
    // EMF 없을 때만 끄기 (매 틱 껐다켰다 금지)
    if (!bCoilActive || CurrentEMF <= 0.f)
    {
        for (const TWeakObjectPtr<AActor>& WirePtr : ConnectedWires)
        {
            if (AWire* W = Cast<AWire>(WirePtr.Get()))
            {
                W->SetBatterySource(false);
                W->SetPowered(false);
            }
        }
        ConnectedWires.Empty();
        return;
    }

    FCollisionQueryParams QParams(SCENE_QUERY_STAT(CoilCircuit), false);
    QParams.AddIgnoredActor(this);

    TArray<FOverlapResult> Hits;
    GetWorld()->OverlapMultiByObjectType(
        Hits, BaseCoilLocation, FQuat::Identity,
        FCollisionObjectQueryParams::AllObjects,
        FCollisionShape::MakeSphere(WireDetectRadius), QParams);

    for (const FOverlapResult& H : Hits)
    {
        AWire* Wire = Cast<AWire>(H.GetActor());
        if (!Wire) continue;

        if (ConnectedWires.Contains(Wire))
        {
            Wire->SetBatteryVoltage(CurrentEMF);  // 이미 연결됨 → 전압만 갱신
            continue;
        }

        Wire->SetBatterySource(true);
        Wire->SetBatteryVoltage(CurrentEMF);
        Wire->SetPowered(true);
        Wire->RefreshConnectedActors();   // ★ 타이머 안 기다리고 즉시 회로 풀기
        ConnectedWires.Add(Wire);
    }
}