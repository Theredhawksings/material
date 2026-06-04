#include "KillZoneVolume.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

AKillZoneVolume::AKillZoneVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	// 박스 콜리전 생성 및 루트 설정
	KillZoneBox = CreateDefaultSubobject<UBoxComponent>(TEXT("KillZoneBox"));
	RootComponent = KillZoneBox;

	// 콜리전 설정 - 폰만 감지
	KillZoneBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	KillZoneBox->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
	KillZoneBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	KillZoneBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);

	// 기본값
	RespawnLocation = FVector::ZeroVector;
	RespawnRotation = FRotator::ZeroRotator;
	RespawnDelay    = 0.0f;
}

void AKillZoneVolume::BeginPlay()
{
	Super::BeginPlay();

	// 오버랩 이벤트 바인딩
	KillZoneBox->OnComponentBeginOverlap.AddDynamic(this, &AKillZoneVolume::OnOverlapBegin);
}

void AKillZoneVolume::OnOverlapBegin(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	ACharacter* PlayerCharacter = Cast<ACharacter>(OtherActor);
	if (!PlayerCharacter)
	{
		return;
	}

	if (RespawnDelay <= 0.0f)
	{
		RespawnPlayer(PlayerCharacter);
	}
	else
	{
		// 딜레이가 있으면 타이머로 처리
		FTimerHandle TimerHandle;
		FTimerDelegate TimerDelegate;
		TimerDelegate.BindUObject(this, &AKillZoneVolume::RespawnPlayer, PlayerCharacter);
		GetWorldTimerManager().SetTimer(TimerHandle, TimerDelegate, RespawnDelay, false);
	}
}

void AKillZoneVolume::RespawnPlayer(ACharacter* PlayerCharacter)
{
	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	// 이동 중지 후 위치 이동
	PlayerCharacter->GetCharacterMovement()->StopMovementImmediately();
	PlayerCharacter->SetActorLocationAndRotation(
		RespawnLocation,
		RespawnRotation,
		false,   // bSweep
		nullptr, // OutSweepHitResult
		ETeleportType::TeleportPhysics
	);
}