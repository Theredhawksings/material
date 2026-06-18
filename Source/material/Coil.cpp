#include "Coil.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"     // ★ 추가
#include "DrawDebugHelpers.h"
#include "Wire.h"
#include "materialCharacter.h"
#include "UObject/ConstructorHelpers.h"    // ★ 추가
#include "Sound/SoundBase.h"               // ★ 추가
#include "Kismet/GameplayStatics.h"
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
	DetectionZone->ShapeColor = FColor::Cyan;
	DetectionZone->bDrawOnlyIfSelected = false;
	DetectionZone->SetHiddenInGame(false);
	DetectionZone->SetVisibility(true);

	// ★ 전선 사운드 로드
	static ConstructorHelpers::FObjectFinder<USoundBase> WireAsset(
		TEXT("/Script/Engine.SoundWave'/Game/Sound/sound_Spark.sound_Spark'"));
	if (WireAsset.Succeeded())
		WireSound = WireAsset.Object;

	// ★ 전선 루프용 오디오 컴포넌트
	WireAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("WireAudioComp"));
	WireAudioComp->SetupAttachment(RootComponent);
	WireAudioComp->bAutoActivate = false;   // 시작 시 자동 재생 X
	if (WireSound)
		WireAudioComp->SetSound(WireSound);
}

void ACoil::BeginPlay()
{
	Super::BeginPlay();

	DetectionZone->SetBoxExtent(DetectionBoxExtent);
	BaseCoilLocation = GetActorLocation();
	CoilMesh->SetSimulatePhysics(false);

	ApplyDebugVisibility();
}

void ACoil::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bCoilActive)
	{
		MagnetsInside.Empty();
		MagnetsInsideLastFrame.Empty();
		CurrentEMF = 0.f;
		ShutdownConnectedWires();
		UpdateWireSound();   // ★ 꺼졌으니 사운드도 정지
		DebugVisualize();
		return;
	}

	// 자석 넣었다 뺐다 감지 → EMF 충전
	UpdateMagnetSensing();

	// EMF 감쇠 (흔들기 멈추면 빨리 식음)
	if (CurrentEMF > 0.f)
		CurrentEMF = FMath::Max(CurrentEMF - EMFDecayRate * DeltaTime, 0.f);

	// 코일이 자석 당기는 힘 (들고 있는 자석 제외)
	ApplyMagneticForce();

	DebugVisualize();
	UpdateCircuit();
	UpdateWireSound();   // ★ 전기 상태에 맞춰 사운드 재생/정지
}

// ============================================================================
//  자석이 DetectionZone 박스 안에 있는지 직접 검사 (충돌 무관, 위치로 판정)
// ============================================================================
bool ACoil::IsActorInsideZone(AActor* Actor) const
{
	if (!Actor || !DetectionZone) return false;

	const FTransform ZoneXform = DetectionZone->GetComponentTransform();
	const FVector LocalPos = ZoneXform.InverseTransformPosition(Actor->GetActorLocation());
	const FVector Extent = DetectionBoxExtent;

	return FMath::Abs(LocalPos.X) <= Extent.X
		&& FMath::Abs(LocalPos.Y) <= Extent.Y
		&& FMath::Abs(LocalPos.Z) <= Extent.Z;
}

// ============================================================================
//  매 프레임 진입/이탈 판정 → "이탈" 순간에만 충전 (넣었다 뺐다 = 1회 발전)
// ============================================================================
void ACoil::UpdateMagnetSensing()
{
	if (bShutdown) return;

	// ── 후보 수집 ──
	TSet<AActor*> Candidates;

	// (A) 플레이어가 손에 든 자석 (충돌 꺼져 있어도 직접 가져옴)
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (AmaterialCharacter* PlayerChar = Cast<AmaterialCharacter>(PC->GetPawn()))
		{
			if (AActor* Held = PlayerChar->GetHeldActor())
			{
				if (Held->ActorHasTag(MagnetTag))
					Candidates.Add(Held);
			}
		}
	}

	// (B) 주변 자석 (바닥에 놓인 것 등)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(CoilMagnetSense), false);
		Q.AddIgnoredActor(this);

		TArray<FOverlapResult> Hits;
		GetWorld()->OverlapMultiByObjectType(
			Hits, DetectionZone->GetComponentLocation(), FQuat::Identity,
			FCollisionObjectQueryParams::AllObjects,
			FCollisionShape::MakeSphere(DetectionBoxExtent.GetMax() * 2.f), Q);

		for (const FOverlapResult& H : Hits)
		{
			AActor* A = H.GetActor();
			if (A && A->ActorHasTag(MagnetTag))
				Candidates.Add(A);
		}
	}

	// ── 진입/이탈 판정 ──
	TSet<TWeakObjectPtr<AActor>> NowInside;
	MagnetsInside.Empty();

	for (AActor* Mag : Candidates)
	{
		const bool bInsideNow = IsActorInsideZone(Mag);

		if (bInsideNow)
		{
			NowInside.Add(Mag);
			MagnetsInside.Add(Mag);   // 당기는 힘/카운트용
		}

		const bool bWasInside = MagnetsInsideLastFrame.Contains(Mag);

		// ★ 안→밖 (이탈) 순간에만 충전 = 넣었다 빼야 1회 발전
		if (!bInsideNow && bWasInside)
		{
			CurrentEMF = FMath::Min(CurrentEMF + EMFPerSwing, MaxEMF);
		}
	}

	MagnetsInsideLastFrame = NowInside;
}

// ============================================================================
//  코일이 자석 당기는 힘 (플레이어가 든 자석은 제외)
// ============================================================================
void ACoil::ApplyMagneticForce()
{
	if (MagnetsInside.Num() == 0) return;

	AActor* HeldByPlayer = nullptr;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		if (AmaterialCharacter* PlayerChar = Cast<AmaterialCharacter>(PC->GetPawn()))
			HeldByPlayer = PlayerChar->GetHeldActor();

	for (const TWeakObjectPtr<AActor>& MagnetPtr : MagnetsInside)
	{
		AActor* Magnet = MagnetPtr.Get();
		if (!Magnet || Magnet == HeldByPlayer) continue;   // 들고 있으면 안 당김

		UPrimitiveComponent* MagnetRoot = Cast<UPrimitiveComponent>(Magnet->GetRootComponent());
		if (!MagnetRoot || !MagnetRoot->IsSimulatingPhysics()) continue;

		const FVector Direction = GetActorLocation() - Magnet->GetActorLocation();
		const float Distance = Direction.Size();
		if (Distance < 1.f) continue;

		const float ForceMag = MagneticForceStrength / (MagnetsInside.Num() * FMath::Max(Distance * Distance, 100.f));
		MagnetRoot->AddForce(Direction.GetSafeNormal() * ForceMag, NAME_None, true);
	}
}

// ============================================================================
//  전선 회로 — EMF 있으면 전압 공급, 없으면 차단
// ============================================================================
void ACoil::UpdateCircuit()
{
	if (!bCoilActive || CurrentEMF <= 0.f)
	{
		ShutdownConnectedWires();
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
			Wire->SetBatteryVoltage(CurrentEMF);
			continue;
		}

		Wire->SetBatterySource(true);
		Wire->SetBatteryVoltage(CurrentEMF);
		Wire->SetPowered(true);
		Wire->RefreshConnectedActors();
		ConnectedWires.Add(Wire);
	}
}

void ACoil::ShutdownConnectedWires()
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
}

// ============================================================================
//  전선 사운드 — 전기 나오는 동안(코일 ON + EMF > 0) 계속 재생, 아니면 정지
// ============================================================================
void ACoil::UpdateWireSound()
{
	if (!WireAudioComp) return;

	const bool bElectricFlowing = bCoilActive && (CurrentEMF > 0.f);

	if (bElectricFlowing && !WireAudioComp->IsPlaying())
	{
		WireAudioComp->Play();
	}
	else if (!bElectricFlowing && WireAudioComp->IsPlaying())
	{
		WireAudioComp->Stop();
	}
}

// ============================================================================
//  켜기 / 끄기 / 영구 정지
// ============================================================================
void ACoil::SetCoilActive(bool bNewActive)
{
	if (bShutdown && bNewActive) return;
	if (bCoilActive == bNewActive) return;
	bCoilActive = bNewActive;
	UE_LOG(LogTemp, Log, TEXT("Coil [%s] -> %s"), *GetName(), bCoilActive ? TEXT("ON") : TEXT("OFF"));

	if (!bCoilActive)
	{
		ShutdownConnectedWires();
		MagnetsInside.Empty();
		MagnetsInsideLastFrame.Empty();
		CurrentEMF = 0.f;
		UpdateWireSound();   // ★ 꺼지면 사운드도 즉시 정지
	}
}

void ACoil::ShutdownCoil()
{
	if (bShutdown) return;
	bShutdown = true;
	SetCoilActive(false);
	UE_LOG(LogTemp, Log, TEXT("Coil [%s] SHUTDOWN - 영구 정지"), *GetName());
}

// ============================================================================
//  디버그
// ============================================================================
void ACoil::DebugVisualize()
{
#if ENABLE_DRAW_DEBUG
	if (!bDebugDraw) return;

	const FVector ZoneLoc    = DetectionZone->GetComponentLocation();
	const FQuat   ZoneQuat   = DetectionZone->GetComponentQuat();
	const FVector ZoneExtent = DetectionZone->GetScaledBoxExtent();

	FColor ZoneColor;
	if (!bCoilActive)        ZoneColor = FColor(40, 40, 40);
	else if (CurrentEMF > 0) ZoneColor = FColor::Green;
	else                     ZoneColor = FColor::Red;

	DrawDebugBox(GetWorld(), ZoneLoc, ZoneExtent, ZoneQuat, ZoneColor, false, 0.f, 0, 3.f);

	FString Status;
	if (!bCoilActive)
		Status = bShutdown ? TEXT("Coil DISABLED") : TEXT("Coil OFF");
	else
		Status = FString::Printf(TEXT("Magnets: %d | EMF: %.1f V"), MagnetsInside.Num(), CurrentEMF);

	DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 80.f),
		Status, nullptr,
		(bCoilActive && CurrentEMF > 0) ? FColor::Green : FColor::Yellow, 0.f, true);
#endif
}

void ACoil::ApplyDebugVisibility()
{
	if (DetectionZone)
	{
		DetectionZone->SetHiddenInGame(!bShowDebugShapes);
		DetectionZone->SetVisibility(bShowDebugShapes);
		DetectionZone->bDrawOnlyIfSelected = !bShowDebugShapes;
		DetectionZone->MarkRenderStateDirty();
	}
}

#if WITH_EDITOR
void ACoil::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName = (PropertyChangedEvent.Property != nullptr)
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (PropName == GET_MEMBER_NAME_CHECKED(ACoil, bShowDebugShapes))
		ApplyDebugVisibility();

	if (PropName == GET_MEMBER_NAME_CHECKED(ACoil, DetectionBoxExtent) && DetectionZone)
		DetectionZone->SetBoxExtent(DetectionBoxExtent);
}
#endif