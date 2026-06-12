#include "MagnetCheckZone.h"
#include "Components/BoxComponent.h"
#include "Transformation_actor.h"
#include "DrawDebugHelpers.h"

AMagnetCheckZone::AMagnetCheckZone()
{
	PrimaryActorTick.bCanEverTick = true;

	CheckBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CheckBox"));
	SetRootComponent(CheckBox);
	CheckBox->SetBoxExtent(CheckBoxExtent);
	CheckBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CheckBox->SetCollisionResponseToAllChannels(ECR_Overlap);
	CheckBox->SetGenerateOverlapEvents(true);
}

void AMagnetCheckZone::BeginPlay()
{
	Super::BeginPlay();
	CheckBox->SetBoxExtent(CheckBoxExtent);
}

void AMagnetCheckZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 이미 통과됐으면 검사 안 함 (문 닫기 없음)
	if (!bSatisfied)
	{
		TArray<AActor*> Overlapping;
		CheckBox->GetOverlappingActors(Overlapping);

		for (AActor* A : Overlapping)
		{
			if (!A || !A->ActorHasTag(MagnetTag)) continue;

			ATransformation_actor* Block = Cast<ATransformation_actor>(A);
			if (!Block) continue;

			if (Block->IsDemagnetized())
			{
				bSatisfied = true;
				OnCheckPassed.Broadcast();   // ★ 문 여는 이벤트, 한 번만
				UE_LOG(LogTemp, Log, TEXT("MagnetCheckZone [%s] PASSED"), *GetName());
				break;
			}
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bDebugDraw)
	{
		DrawDebugBox(GetWorld(), GetActorLocation(),
			CheckBox->GetScaledBoxExtent(), CheckBox->GetComponentQuat(),
			bSatisfied ? FColor::Green : FColor::Red, false, 0.f, 0, 2.f);

		DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 80.f),
			bSatisfied ? TEXT("PASSED") : TEXT("Need: Demagnetized Magnet"),
			nullptr, bSatisfied ? FColor::Green : FColor::Red, 0.f, true);
	}
#endif
}