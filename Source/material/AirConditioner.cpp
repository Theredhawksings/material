#include "AirConditioner.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"

AAirConditioner::AAirConditioner()
{
    PrimaryActorTick.bCanEverTick = true;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(
        TEXT("/Game/modeling/Object/air_condition/air_conditioning.air_conditioning"));
    if (MeshFinder.Succeeded() && MeshComp)
        MeshComp->SetStaticMesh(MeshFinder.Object);

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
        TEXT("/Game/modeling/Object/air_condition/M_Air_condition.M_Air_condition"));
    if (MatFinder.Succeeded() && MeshComp)
        MeshComp->SetMaterial(0, MatFinder.Object);

    if (MeshComp)
    {
        MeshComp->SetCollisionProfileName(TEXT("BlockAll"));
        MeshComp->SetRelativeScale3D(FVector(300.f, 300.f, 300.f));
    }

    Temperature = 0.f;
    CoolRate = 0.f;
}

void AAirConditioner::BeginPlay()
{
    Super::BeginPlay();

    if (bAlwaysOn)
        ActivateAircon();
    else
    {
        Temperature = 0.f;
        bIsRunning = false;
    }
}

void AAirConditioner::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsRunning)
        HeatNearbyTemperatureBlocks(DeltaTime);

    if (GEngine)
    {
        FString DebugMsg = FString::Printf(
            TEXT("=== 에어컨(히터) ===\n작동: %s\n상시활성화: %s\n에어컨온도: %.1f\n"),
            bIsRunning ? TEXT("ON") : TEXT("OFF"),
            bAlwaysOn ? TEXT("ON") : TEXT("OFF"),
            Temperature);

        GEngine->AddOnScreenDebugMessage(2, 0.1f, FColor::Cyan, DebugMsg);
    }
}

void AAirConditioner::HeatNearbyTemperatureBlocks(float DeltaTime)
{
    if (!HeatSphere) return;

    TArray<AActor*> OverlappingActors;
    HeatSphere->GetOverlappingActors(OverlappingActors);

    for (AActor* Actor : OverlappingActors)
    {
        if (!Actor || Actor == this) continue;

        // ── ATemperature 블록만 대상 ──
        ATemperature* TempBlock = Cast<ATemperature>(Actor);
        if (!TempBlock) continue;

        // ── Stefan-Boltzmann 복사열 기반 수열량 (W) ──
        const float ReceivedW = GetReceivedPowerW(
            TempBlock->GetActorLocation(),
            BlockReceiverAreaM2);

        if (ReceivedW <= 0.f) continue;

        // ── ΔT = (P * Δt * TimeScale) / (m * c) ──
        const float DeltaT = (ReceivedW * DeltaTime * HeatSimTimeScale)
                           / (BlockMassKg * BlockSpecificHeatJPerKgK);

        TempBlock->Temperature = FMath::Min(
            TempBlock->Temperature + DeltaT,
            HeatTemperature);

        if (bDebugHeat)
        {
            GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Orange,
                FString::Printf(TEXT("[가열] %s → ReceivedW: %.2fW | ΔT: %.4f | 현재: %.1f℃"),
                    *Actor->GetName(), ReceivedW, DeltaT, TempBlock->Temperature));
        }
    }
}

void AAirConditioner::ActivateAircon()
{
    if (bIsRunning) return;

    bIsRunning = true;
    Temperature = HeatTemperature;

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
        TEXT("[에어컨] 작동 시작 (ON)"));
}

void AAirConditioner::DeactivateAircon()
{
    if (bAlwaysOn) return;
    if (!bIsRunning) return;

    bIsRunning = false;
    Temperature = 0.f;

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::White,
        TEXT("[에어컨] 작동 정지 (OFF)"));
}