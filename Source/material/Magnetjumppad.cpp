#include "MagnetJumpPad.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

AMagnetJumpPad::AMagnetJumpPad()
{
	PrimaryActorTick.bCanEverTick = true;

	PadRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PadRoot"));
	SetRootComponent(PadRoot);

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(PadRoot);

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(PadRoot);
	PadMesh->SetRelativeLocation(FVector(-RestHeight, 0, 0));

	DetectRange = CreateDefaultSubobject<USphereComponent>(TEXT("DetectRange"));
	DetectRange->SetupAttachment(PadMesh);
	DetectRange->InitSphereRadius(PlayerDetectRadius);
	DetectRange->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
}

void AMagnetJumpPad::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (PadMesh)
		PadMesh->SetRelativeLocation(FVector(-RestHeight, 0, 0));
}

void AMagnetJumpPad::BeginPlay()
{
	Super::BeginPlay();
	CachedPlayer = UGameplayStatics::GetPlayerCharacter(this, 0);
	PadZ = RestHeight;
	PadVel = 0.f;
	bLaunched = false;
	if (PadMesh) PadMesh->SetRelativeLocation(FVector(-RestHeight, 0, 0));
}

void AMagnetJumpPad::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float Dt = FMath::Min(DeltaTime, 1.f / 30.f);
	const bool bStepped = IsPlayerOnPad();

	// 복원 스프링
	float Accel = -RestStiffness * (PadZ - RestHeight);
	if (bStepped) Accel -= PlayerPressForce;
	Accel -= PadVel * Damping;

	PadVel += Accel * Dt;
	PadZ   += PadVel * Dt;

	// 범위 제한
	const float LoZ = RestHeight - TravelRange;
	const float HiZ = RestHeight + TravelRange;
	if (PadZ < LoZ) { PadZ = LoZ; PadVel = 0.f; }
	if (PadZ > HiZ) { PadZ = HiZ; PadVel = 0.f; }

	// 발판 위치 갱신
	if (PadMesh) PadMesh->SetRelativeLocation(FVector(-PadZ, 0, 0));

	// ---- 자동 점프: 최대 압축(LoZ)에 닿으면 발사 ----
	const bool bAtBottom = FMath::IsNearlyEqual(PadZ, LoZ, 1.f);
	if (bStepped && bAtBottom && !bLaunched)
	{
		if (CachedPlayer)
		{
			const FVector LaunchVel = FVector(0, 0, FMath::Clamp(LaunchSpeed, MinLaunchSpeed, MaxLaunchSpeed));
			CachedPlayer->LaunchCharacter(LaunchVel, false, true);
		}
		bLaunched = true;
	}

	// 발판이 복귀하면 다시 발사 가능
	if (!bStepped) bLaunched = false;

	if (bDebugDraw)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Cyan,
			FString::Printf(TEXT("PadZ: %.1f | LoZ: %.1f | Stepped: %s"),
				PadZ, LoZ, bStepped ? TEXT("YES") : TEXT("NO")));
	}
}

bool AMagnetJumpPad::IsPlayerOnPad() const
{
	if (!CachedPlayer || !PadMesh) return false;

	const FVector PadW     = PadMesh->GetComponentLocation();
	const FVector Up       = GetActorUpVector();
	const FVector ToPlayer = CachedPlayer->GetActorLocation() - PadW;

	const float Along = FVector::DotProduct(ToPlayer, Up);
	if (Along < -20.f || Along > PlayerDetectHeight) return false;

	const FVector Radial = ToPlayer - Up * Along;
	return Radial.Size() <= PlayerDetectRadius;
}