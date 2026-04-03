#include "Coil.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"

ACoil::ACoil()
{
	PrimaryActorTick.bCanEverTick = true;

	// ── 코일 메시 ──
	CoilMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoilMesh"));
	SetRootComponent(CoilMesh);
	CoilMesh->SetMobility(EComponentMobility::Movable);
	CoilMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	CoilMesh->SetGenerateOverlapEvents(true);

	// ── 감지 박스 ──
	DetectionZone = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionZone"));
	DetectionZone->SetupAttachment(RootComponent);
	DetectionZone->SetBoxExtent(DetectionBoxExtent);
	DetectionZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectionZone->SetCollisionResponseToAllChannels(ECR_Overlap);
	DetectionZone->SetGenerateOverlapEvents(true);
	DetectionZone->SetHiddenInGame(false);
	DetectionZone->ShapeColor = FColor::Cyan;

	// ── 자기장 구체 ──
	MagneticFieldSphere = CreateDefaultSubobject<USphereComponent>(TEXT("MagneticFieldSphere"));
	MagneticFieldSphere->SetupAttachment(RootComponent);
	MagneticFieldSphere->SetSphereRadius(MagneticFieldRadius);
	MagneticFieldSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MagneticFieldSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
	MagneticFieldSphere->SetGenerateOverlapEvents(true);
	MagneticFieldSphere->SetHiddenInGame(false);
	MagneticFieldSphere->ShapeColor = FColor::Blue;
}

void ACoil::BeginPlay()
{
	Super::BeginPlay();

	DetectionZone->SetBoxExtent(DetectionBoxExtent);
	MagneticFieldSphere->SetSphereRadius(MagneticFieldRadius);
	BaseCoilLocation = GetActorLocation();

	CoilMesh->SetSimulatePhysics(false);
}

void ACoil::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	DetectMagnets();
	ApplyOscillation(DeltaTime);
	UpdateFieldRadius();
	ApplyMagneticForce();
	DebugVisualize();
}

// ──────────────────────────────────────────────
//  자석 감지
// ──────────────────────────────────────────────
void ACoil::DetectMagnets()
{
	DetectedMagnets.Empty();

	if (!DetectionZone) return;

	TArray<FOverlapResult> Hits;
	FCollisionQueryParams QParams(SCENE_QUERY_STAT(CoilDetect), false);
	QParams.AddIgnoredActor(this);

	GetWorld()->OverlapMultiByObjectType(
		Hits,
		DetectionZone->GetComponentLocation(),
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
}

// ──────────────────────────────────────────────
//  코일 진동 (자석 수에 비례)
// ──────────────────────────────────────────────
void ACoil::ApplyOscillation(float DeltaTime)
{
	if (HasMagnetInside())
	{
		OscillationTime += DeltaTime;

		const int32 Count = DetectedMagnets.Num();

		// 자석이 많을수록 진폭 ↑, 속도 ↑
		const float ScaledAmplitude = OscillationAmplitude * Count;
		const float ScaledSpeed = OscillationSpeed + (Count - 1) * SpeedPerExtraMagnet;

		const float OffsetZ = FMath::Sin(OscillationTime * ScaledSpeed) * ScaledAmplitude;
		const FVector NewLoc = BaseCoilLocation + FVector(0.f, 0.f, OffsetZ);
		SetActorLocation(NewLoc, false);

		// 자기장 구체는 원래 위치에 고정
		MagneticFieldSphere->SetWorldLocation(BaseCoilLocation);
	}
	else
	{
		OscillationTime = 0.f;
		SetActorLocation(BaseCoilLocation, false);
	}
}

// ──────────────────────────────────────────────
//  자기장 반경 동적 업데이트
// ──────────────────────────────────────────────
void ACoil::UpdateFieldRadius()
{
	const float DynamicRadius = HasMagnetInside()
		? MagneticFieldRadius + (DetectedMagnets.Num() - 1) * FieldRadiusPerMagnet
		: MagneticFieldRadius;

	MagneticFieldSphere->SetSphereRadius(DynamicRadius);
}

// ──────────────────────────────────────────────
//  자석에 인력 적용 (거리 반비례)
// ──────────────────────────────────────────────
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

		// F = Strength / d²  (최소 거리 제한으로 무한대 방지)
		const float ForceMag = MagneticForceStrength / FMath::Max(Distance * Distance, 100.f);
		const FVector Force = Direction.GetSafeNormal() * ForceMag;

		MagnetRoot->AddForce(Force, NAME_None, true);
	}
}

// ──────────────────────────────────────────────
//  디버그 시각화
// ──────────────────────────────────────────────
void ACoil::DebugVisualize()
{
#if ENABLE_DRAW_DEBUG
	if (!bDebugDraw) return;

	const bool bActive = HasMagnetInside();

	// 감지 박스
	const FColor BoxColor = bActive ? FColor::Green : FColor::Red;
	DrawDebugBox(GetWorld(), DetectionZone->GetComponentLocation(),
		DetectionBoxExtent, DetectionZone->GetComponentQuat(),
		BoxColor, false, 0.f, 0, 2.f);

	// 자기장 구체 (동적 반경 반영)
	const float CurrentRadius = MagneticFieldSphere->GetScaledSphereRadius();
	const FColor SphereColor = bActive ? FColor::Blue : FColor(80, 80, 80);
	DrawDebugSphere(GetWorld(), MagneticFieldSphere->GetComponentLocation(),
		CurrentRadius, 24, SphereColor, false, 0.f, 0, 1.f);

	// 상태 텍스트
	if (bActive)
	{
		DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 60.f),
			FString::Printf(TEXT("Magnets: %d | Radius: %.0f | Field ON"),
				DetectedMagnets.Num(), CurrentRadius),
			nullptr, FColor::Green, 0.f, true);
	}

	// 자석별 인력 방향 화살표
	for (const TWeakObjectPtr<AActor>& MagnetPtr : DetectedMagnets)
	{
		if (AActor* M = MagnetPtr.Get())
		{
			DrawDebugDirectionalArrow(GetWorld(),
				M->GetActorLocation(),
				BaseCoilLocation,
				20.f, FColor::Yellow, false, 0.f, 0, 2.f);
		}
	}
#endif
}