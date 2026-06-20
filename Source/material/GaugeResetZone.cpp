#include "GaugeResetZone.h"
#include "Components/BoxComponent.h"
#include "materialCharacter.h"

AGaugeResetZone::AGaugeResetZone()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);
	TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));
	TriggerBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AGaugeResetZone::OnZoneBeginOverlap);
}

void AGaugeResetZone::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (bHasTriggered || !OtherActor) return;

	AmaterialCharacter* Character = Cast<AmaterialCharacter>(OtherActor);
	if (!Character) return;

	Character->ResetAllGauges();
	bHasTriggered = true;   // ★ 이후 재진입해도 발동 안 됨

	UE_LOG(LogTemp, Warning, TEXT("[GaugeResetZone] %s 통과 - 게이지 리셋"), *OtherActor->GetName());
}