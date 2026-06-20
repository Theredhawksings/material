#include "MagnetJumpPad.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

AMagnetJumpPad::AMagnetJumpPad()
{
	PrimaryActorTick.bCanEverTick = true;

	PadRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PadRoot"));
	SetRootComponent(PadRoot);

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(PadRoot);

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(PadRoot);

	// DetectRange는 PadMesh에 부착 → 발판과 함께 이동
	DetectRange = CreateDefaultSubobject<USphereComponent>(TEXT("DetectRange"));
	DetectRange->SetupAttachment(PadMesh);
	DetectRange->InitSphereRadius(PlayerDetectRadius);
	DetectRange->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
}

void AMagnetJumpPad::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (DetectRange)
		DetectRange->SetSphereRadius(PlayerDetectRadius);

	if (PadMesh)
		PadMesh->SetRelativeLocation(GetActorUpVector() * -RestHeight);
}

void AMagnetJumpPad::BeginPlay()
{
	Super::BeginPlay();

	PadZ      = RestHeight;
	PadVel    = 0.f;
	bLaunched = false;
	bStepped  = false;

	if (PadMesh)
		PadMesh->SetRelativeLocation(GetActorUpVector() * -PadZ);

	if (DetectRange)
	{
		DetectRange->OnComponentBeginOverlap.AddDynamic(this, &AMagnetJumpPad::OnDetectBeginOverlap);
		DetectRange->OnComponentEndOverlap.AddDynamic(this,   &AMagnetJumpPad::OnDetectEndOverlap);
	}
}

// ── 오버랩 콜백 ────────────────────────────────────────────────────────────────

void AMagnetJumpPad::OnDetectBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/,
	AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (ACharacter* Char = Cast<ACharacter>(OtherActor))
	{
		CachedPlayer = Char;
		bStepped     = true;
	}
}

void AMagnetJumpPad::OnDetectEndOverlap(UPrimitiveComponent* /*OverlappedComp*/,
	AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	if (OtherActor == CachedPlayer)
	{
		bStepped  = false;
		bLaunched = false;
	}
}

// ── Tick ───────────────────────────────────────────────────────────────────────

void AMagnetJumpPad::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float Dt = FMath::Min(DeltaTime, 1.f / 30.f);

	// ── 스프링 물리 ──
	float Accel = -RestStiffness * (PadZ - RestHeight);
	if (bStepped)
		Accel -= PlayerPressForce;
	Accel -= PadVel * Damping;

	PadVel += Accel * Dt;
	PadZ   += PadVel * Dt;

	// ── 이동 범위 제한 ──
	const float LoZ = RestHeight - TravelRange;
	const float HiZ = RestHeight + TravelRange;
	if (PadZ < LoZ) { PadZ = LoZ; PadVel = 0.f; }
	if (PadZ > HiZ) { PadZ = HiZ; PadVel = 0.f; }

	// ── 발판 위치 갱신 (액터 Up 방향 기준으로 내려감) ──
	if (PadMesh)
	{
		const FVector Offset = GetActorUpVector() * -PadZ;
		PadMesh->SetRelativeLocation(Offset);
	}

	// ── 자동 점프: 최대 압축 시 발사 ──
	const bool bAtBottom = FMath::IsNearlyEqual(PadZ, LoZ, 1.f);
	if (bStepped && bAtBottom && !bLaunched && CachedPlayer)
	{
		const FVector LaunchVel = FVector(0, 0,
			FMath::Clamp(LaunchSpeed, MinLaunchSpeed, MaxLaunchSpeed));
		CachedPlayer->LaunchCharacter(LaunchVel, false, true);
		bLaunched = true;
	}

	// ── 디버그 ──
	if (bDebugDraw && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Cyan,
			FString::Printf(TEXT("[JumpPad] PadZ: %.1f | LoZ: %.1f | Stepped: %s | Launched: %s"),
				PadZ, LoZ,
				bStepped  ? TEXT("YES") : TEXT("NO"),
				bLaunched ? TEXT("YES") : TEXT("NO")));
	}
}