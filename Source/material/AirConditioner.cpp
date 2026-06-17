#include "AirConditioner.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/AudioComponent.h" // ★ 추가
#include "Sound/SoundBase.h"           // ★ 추가
#include "TimerManager.h"              // ★ 추가
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

AAirConditioner::AAirConditioner()
{
    PrimaryActorTick.bCanEverTick = true;

    // ── 본체 메시 ──
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

    // ★ wireframe 메시 컴포넌트 추가
    WireframeMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WireframeMeshComp"));
    WireframeMeshComp->SetupAttachment(MeshComp);
    WireframeMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WireframeMeshComp->SetRelativeScale3D(FVector(1.f, 1.f, 1.f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> WireMeshFinder(
        TEXT("/Game/modeling/Object/air_condition/wireframe.wireframe"));
    if (WireMeshFinder.Succeeded())
        WireframeMeshComp->SetStaticMesh(WireMeshFinder.Object);

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> WireMatFinder(
        TEXT("/Game/modeling/Object/air_condition/M_Air_condition_wire.M_Air_condition_wire"));
    if (WireMatFinder.Succeeded())
        WireframeMeshComp->SetMaterial(0, WireMatFinder.Object);

    // ★ 연기 이펙트 기본값 (얼음 수증기와 동일한 Smoke 사용)
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SmokeFX(
        TEXT("/Game/modeling/Effect/Smoke.Smoke"));
    if (SmokeFX.Succeeded())
        SmokeEffect = SmokeFX.Object;

    // ★ 연기 노즐 9개 생성 (본체에 부착, 위치는 에디터에서 조정)
    SmokeComponents.Reserve(NumSmokeNozzles);
    for (int32 i = 0; i < NumSmokeNozzles; ++i)
    {
        const FString CompName = FString::Printf(TEXT("SmokeNozzle_%d"), i);
        UNiagaraComponent* Nozzle =
            CreateDefaultSubobject<UNiagaraComponent>(*CompName);

        Nozzle->SetupAttachment(MeshComp);
        Nozzle->SetCollisionEnabled(ECollisionEnabled::NoCollision);

        // 기본 위치를 살짝 흩어놓음 (어차피 에디터에서 옮길 예정)
        Nozzle->SetRelativeLocation(FVector(0.f, 0.f, i * 5.f));

        if (SmokeEffect)
            Nozzle->SetAsset(SmokeEffect);

        // 시작 시엔 자동 활성화하지 않음
        Nozzle->bAutoActivate = false;

        SmokeComponents.Add(Nozzle);
    }

    // ★ 추가: 스모크 사운드 자동 로드
    static ConstructorHelpers::FObjectFinder<USoundBase> SmokeSnd(
        TEXT("/Game/Sound/sound_gas_injection.sound_gas_injection"));
    if (SmokeSnd.Succeeded())
        SmokeSound = SmokeSnd.Object;

    // ★ 추가: 스모크 사운드용 오디오 컴포넌트 생성
    SmokeAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("SmokeAudioComp"));
    SmokeAudioComp->SetupAttachment(MeshComp);
    SmokeAudioComp->bAutoActivate = false; // 시작 시 자동 재생 안 함
    if (SmokeSound)
        SmokeAudioComp->SetSound(SmokeSound);

    Temperature = 0.f;
    CoolRate = 0.f;
}

void AAirConditioner::BeginPlay()
{
    Super::BeginPlay();

    // 시작 시 연기 전부 꺼두기
    SetSmokeActive(false);

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

    // ★ WireframeMeshComp stencil 동기화
    if (WireframeMeshComp)
    {
        const float Ratio = FMath::Clamp(Temperature / FMath::Max(MaxStencilTemperature, 1.f), 0.f, 1.f);
        const int32 StencilValue = FMath::RoundToInt(Ratio * 255.f);
        WireframeMeshComp->SetCustomDepthStencilValue(StencilValue);
        WireframeMeshComp->SetRenderCustomDepth(Temperature > 1.f);
    }

    if (GEngine)
    {
        FString DebugMsg = FString::Printf(
            TEXT("=== 에어컨(히터) ===\n작동: %s\n상시활성화: %s\n에어컨온도: %.1f\n"),
            bIsRunning ? TEXT("ON") : TEXT("OFF"),
            bAlwaysOn ? TEXT("ON") : TEXT("OFF"),
            Temperature);

        GEngine->AddOnScreenDebugMessage(2, 0.1f, FColor::Cyan, DebugMsg);
    }

    if (bShowDebugShapes && HeatSphere)
    {
        GEngine->AddOnScreenDebugMessage(
            static_cast<int32>(GetUniqueID()) + 1, 0.f, FColor::Red,
            FString::Printf(TEXT("[%s] MaxHeatDist: %.1f | SphereR: %.1f"),
                *GetName(), MaxHeatDistance, HeatSphere->GetScaledSphereRadius()));
    }
}

void AAirConditioner::HeatNearbyTemperatureBlocks(float DeltaTime)
{
    if (!HeatSphere) return;

    TArray<AActor*> OverlappingActors;
    HeatSphere->GetOverlappingActors(OverlappingActors);

    static const FName StartHeatingName(TEXT("StartHeating"));
    static const FName IsHeatingName(TEXT("IsHeating"));

    for (AActor* Actor : OverlappingActors)
    {
        if (!Actor || Actor == this) continue;
        if (Actor->IsA<AAirConditioner>()) continue;

        // ★ 얼음(또는 변신 블럭): StartHeating으로 자신을 가열원 등록
        if (UFunction* StartFn = Actor->FindFunction(StartHeatingName))
        {
            bool bAlreadyHeating = false;
            if (UFunction* CheckFn = Actor->FindFunction(IsHeatingName))
            {
                Actor->ProcessEvent(CheckFn, &bAlreadyHeating);
            }

            if (!bAlreadyHeating)
            {
                struct FArgs { ATemperature* FireRef; };
                FArgs Args{ this };
                Actor->ProcessEvent(StartFn, &Args);
            }
            continue;
        }

        // ── 그 외 일반 ATemperature 블럭: 기존 방식대로 직접 온도 주입 ──
        ATemperature* TempBlock = Cast<ATemperature>(Actor);
        if (!TempBlock) continue;

        const float ReceivedW = GetReceivedPowerW(
            TempBlock->GetActorLocation(),
            BlockReceiverAreaM2);

        if (ReceivedW <= 0.f) continue;

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

void AAirConditioner::SetSmokeActive(bool bActive)
{
    for (UNiagaraComponent* Nozzle : SmokeComponents)
    {
        if (!Nozzle) continue;

        if (bActive)
        {
            // 얼음 수증기와 동일하게 SpawnRate 변수 세팅
            Nozzle->SetVariableFloat(FName(TEXT("SpawnRate")), SmokeSpawnRate);
            Nozzle->Activate(true);
        }
        else
        {
            // 즉시 끊지 않고 완만하게 비활성화 (남은 입자는 자연 소멸)
            Nozzle->Deactivate();
        }
    }

    // ★ 추가: 사운드 처리
    if (bActive)
    {
        // 스모크 작동 → 0.3초 뒤 사운드 재생
        GetWorldTimerManager().SetTimer(
            SmokeSoundTimer, this, &AAirConditioner::PlaySmokeSound, SmokeSoundDelay, false);
    }
    else
    {
        // 스모크 꺼짐 → 대기 중인 타이머 취소 + 사운드 정지
        GetWorldTimerManager().ClearTimer(SmokeSoundTimer);
        if (SmokeAudioComp && SmokeAudioComp->IsPlaying())
        {
            SmokeAudioComp->Stop();
        }
    }
}

// ★ 추가: 스모크 사운드 재생 (타이머에서 호출)
void AAirConditioner::PlaySmokeSound()
{
    if (SmokeAudioComp && SmokeSound)
    {
        SmokeAudioComp->Play();
    }
}

void AAirConditioner::ActivateAircon()
{
    if (bIsRunning) return;

    bIsRunning = true;
    Temperature = HeatTemperature;

    SetSmokeActive(true);   // ★ 연기 ON (+ 0.3초 뒤 사운드)

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
        TEXT("[에어컨] 작동 시작 (ON)"));
}

void AAirConditioner::DeactivateAircon()
{
    if (bAlwaysOn) return;
    if (!bIsRunning) return;

    bIsRunning = false;
    Temperature = 0.f;

    SetSmokeActive(false);  // ★ 연기 OFF (+ 사운드 정지)

    // ★ 범위 안 얼음(및 모든 가열 대상)에게 StopHeating 전송
    if (HeatSphere)
    {
        TArray<AActor*> OverlappingActors;
        HeatSphere->GetOverlappingActors(OverlappingActors);

        static const FName StopHeatingName(TEXT("StopHeating"));
        for (AActor* Actor : OverlappingActors)
        {
            if (!Actor || Actor == this) continue;
            if (Actor->IsA<AAirConditioner>()) continue;

            if (UFunction* StopFn = Actor->FindFunction(StopHeatingName))
            {
                Actor->ProcessEvent(StopFn, nullptr);
            }
        }
    }

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::White,
        TEXT("[에어컨] 작동 정지 (OFF)"));
}