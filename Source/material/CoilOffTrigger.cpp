#include "CoilOffTrigger.h"
#include "GameFramework/Character.h"
#include "Coil.h"

ACoilOffTrigger::ACoilOffTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void ACoilOffTrigger::BeginPlay()
{
	Super::BeginPlay();
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACoilOffTrigger::OnOverlapBegin);
}

void ACoilOffTrigger::OnOverlapBegin(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	ACharacter* OverlappedCharacter = Cast<ACharacter>(OtherActor);
	if (!OverlappedCharacter || !OverlappedCharacter->IsPlayerControlled()) return;

	for (ACoil* Coil : TargetCoils)
	{
		if (Coil && !Coil->IsShutdown())
			Coil->ShutdownCoil();   // 영구 정지
	}

	// 한 번 발동했으면 트리거 자체도 비활성화 (1회용)
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}