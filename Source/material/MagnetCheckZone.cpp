#include "MagnetCheckZone.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Transformation_actor.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"

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

	// 문 시작/목표 위치 계산 (Platform1과 동일 방식: 좌우 반대 방향)
	const FVector NormDir = OpenDirection.GetSafeNormal();

	if (LeftDoorActor)
	{
		LeftStartLocation  = LeftDoorActor->GetActorLocation();
		LeftTargetLocation = LeftStartLocation + NormDir * OpenDistance;
	}
	if (RightDoorActor)
	{
		RightStartLocation  = RightDoorActor->GetActorLocation();
		RightTargetLocation = RightStartLocation - NormDir * OpenDistance;
	}
}

void AMagnetCheckZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ── 조건 검사: 자성 잃은 자석이 존 안에 있는가 (한 번만) ──
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
				bIsOpening = true;

				if (GEngine)
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
						TEXT("자성 소실 감지! 문이 열립니다"));

				// 문 충돌 해제 (Platform1과 동일)
				auto DisableCollision = [](AActor* Door)
				{
					if (!Door) return;
					TArray<UStaticMeshComponent*> Meshes;
					Door->GetComponents<UStaticMeshComponent>(Meshes);
					for (UStaticMeshComponent* M : Meshes)
						M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				};
				DisableCollision(LeftDoorActor);
				DisableCollision(RightDoorActor);
				break;
			}
		}
	}

	// ── 문 열기 애니메이션 (한 번 열리면 끝, 닫기 없음) ──
	if (bIsOpening && !bIsOpen)
	{
		CurrentTime += DeltaTime * OpenSpeed;
		if (CurrentTime >= 1.0f)
		{
			CurrentTime = 1.0f;
			bIsOpen     = true;
			bIsOpening  = false;
			SetActorTickEnabled(false);   // 다 열렸으면 틱 정지
		}

		if (LeftDoorActor)
			LeftDoorActor->SetActorLocation(
				FMath::Lerp(LeftStartLocation, LeftTargetLocation, CurrentTime));

		if (RightDoorActor)
			RightDoorActor->SetActorLocation(
				FMath::Lerp(RightStartLocation, RightTargetLocation, CurrentTime));
	}

#if ENABLE_DRAW_DEBUG
	if (bDebugDraw && !bIsOpen)
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