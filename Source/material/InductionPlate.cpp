#include "InductionPlate.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Transformation_actor.h"
#include "Engine/World.h"
#include "Wire.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"

AInductionPlate::AInductionPlate()
{
	PrimaryActorTick.bCanEverTick = true;

	PlateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlateMesh"));
	SetRootComponent(PlateMesh);
	PlateMesh->SetMobility(EComponentMobility::Movable);
	PlateMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	PlateMesh->SetGenerateOverlapEvents(true);
}

void AInductionPlate::BeginPlay()
{
	Super::BeginPlay();
	TemperatureC = 20.f;
}

void AInductionPlate::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 1. 열화상 스텐실 실시간 업데이트
    if (PlateMesh)
    {
        float TempRatio = FMath::Clamp((TemperatureC - 20.f) / 780.f, 0.f, 1.f);
        int32 StencilVal = FMath::RoundToInt(TempRatio * 255.f);
        
        // 0보다 크면 켜고, 0이면 꺼서 실시간으로 낮아지도록 반영
        PlateMesh->SetRenderCustomDepth(StencilVal > 0);
        PlateMesh->SetCustomDepthStencilValue(StencilVal);
    }

    // 2. 전선 연결 및 가열 로직 (기존 유지)
    int32 ConnectedWireCount = 0;
    float LastPowerW = 0.f;
    float LastEnergyAdded = 0.f;
    bool bIsHeating = false; // 가열 중인지 체크하기 위한 변수

    {
        TArray<FOverlapResult> Hits;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(PlateWireSense), false);
        Q.AddIgnoredActor(this);

        GetWorld()->OverlapMultiByObjectType(
            Hits, GetActorLocation(), FQuat::Identity,
            FCollisionObjectQueryParams::AllObjects,
            FCollisionShape::MakeSphere(WireConnectRadius * 2.f), Q);

        TSet<AWire*> ProcessedWires;
        for (const FOverlapResult& H : Hits)
        {
            AWire* Wire = Cast<AWire>(H.GetActor());
            if (!Wire || ProcessedWires.Contains(Wire)) continue;
            ProcessedWires.Add(Wire);

            const bool bPow = Wire->IsPowered();
            const float V = Wire->GetEffectiveVoltage();
            const float I = Wire->GetEffectiveCurrent();
            const float DistStart = FVector::Dist(Wire->GetStartPointLocation(), GetActorLocation());
            const float DistEnd   = FVector::Dist(Wire->GetEndPointLocation(),   GetActorLocation());
            const float MinDist = FMath::Min(DistStart, DistEnd);

            if (!bPow || V <= 0.f || MinDist > WireConnectRadius) continue;

            const float PowerW = V * I;
            const float Energy = PowerW * WireHeatingRate * DeltaTime;
            
            ReceiveInductionHeat(Energy);
            bIsHeating = true; // 열을 공급받고 있음!

            ConnectedWireCount++;
            LastPowerW = PowerW;
            LastEnergyAdded = FMath::Min(Energy, PlateMaxRisePerCall);
        }
    }

    // 3. 자석 / 철 블록으로 열 전달 (감지 위치 및 디버그 보완)
    if (TemperatureC > 20.f)
    {
        TArray<FOverlapResult> Hits;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(PlateHeat), false);
        Q.AddIgnoredActor(this);

        // 피봇이 바닥일 경우를 대비해 살짝 위쪽을 중심으로 구체 생성
        FVector DetectionCenter = GetActorLocation() + FVector(0.f, 0.f, 20.f);

#if ENABLE_DRAW_DEBUG
        // 에디터에서 열 전달 감지 범위를 시각적으로 확인 (녹색 구체)
        if (bDebugDraw)
        {
            DrawDebugSphere(GetWorld(), DetectionCenter, HeatTransferRadius, 16, FColor::Green, false, -1.f, 0, 0.5f);
        }
#endif

        GetWorld()->OverlapMultiByObjectType(
            Hits, DetectionCenter, FQuat::Identity,
            FCollisionObjectQueryParams::AllObjects,
            FCollisionShape::MakeSphere(HeatTransferRadius), Q);

        TSet<ATransformation_actor*> HeatedBlocks;

        for (const FOverlapResult& H : Hits)
        {
            ATransformation_actor* Block = Cast<ATransformation_actor>(H.GetActor());
            if (!Block || HeatedBlocks.Contains(Block)) continue;

            // 태그가 대소문자를 타거나 누락되었는지 체크하기 위해 디버그 로그 추천
            if (!Block->ActorHasTag(TEXT("Metal")) && !Block->ActorHasTag(TEXT("Magnet")))
                continue;

            // 정상 감지됨 -> 열 전달
            float Energy = (TemperatureC - 20.f) * HeatTransferRate * DeltaTime;
            Energy = FMath::Min(Energy, BlockMaxRisePerCall);
            Block->AddFormHeat(Energy);

            HeatedBlocks.Add(Block);
        }
    }

    // 4. 자연 냉각 (전선으로 가열 중이 아닐 때만 식도록 조건 제어 가능)
    // 만약 가열 중에도 식어야 한다면 조건문을 지우되, PlateCoolingRatePerSec 값을 낮춰보세요.
    if (!bIsHeating && TemperatureC > 20.f)
    {
        TemperatureC = FMath::Max(TemperatureC - PlateCoolingRatePerSec * DeltaTime, 20.f);
    }

#if ENABLE_DRAW_DEBUG
    if (bDebugDraw && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Yellow,
            FString::Printf(TEXT("[Plate] Temp:%.1f | 연결전선:%d | P=V*I:%.2f | 스텐실 값:%d"),
                TemperatureC, ConnectedWireCount, LastPowerW, FMath::RoundToInt(FMath::Clamp((TemperatureC - 20.f) / 780.f, 0.f, 1.f) * 255.f)));
    }
#endif
}

void AInductionPlate::ReceiveInductionHeat(float EnergyJ)
{
	if (EnergyJ <= 0.f) return;

	// 한 번에 올라갈 수 있는 온도 상한 (급상승 방지)
	EnergyJ = FMath::Min(EnergyJ, PlateMaxRisePerCall);

	TemperatureC += EnergyJ;
}