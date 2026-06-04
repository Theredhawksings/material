#include "KillZoneVolume.h"

AKillZoneVolume::AKillZoneVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	KillZoneBox = CreateDefaultSubobject<UBoxComponent>(TEXT("KillZoneBox"));
	RootComponent = KillZoneBox;

	KillZoneBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	KillZoneBox->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
	KillZoneBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	KillZoneBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);

	RespawnLocation = FVector::ZeroVector;
	RespawnRotation = FRotator::ZeroRotator;
	RespawnDelay    = 0.0f;
}

void AKillZoneVolume::BeginPlay()
{
	Super::BeginPlay();
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

	PlayerCharacter->GetCharacterMovement()->StopMovementImmediately();
	PlayerCharacter->SetActorLocationAndRotation(
		RespawnLocation,
		RespawnRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);
}