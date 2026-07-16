#include "AirConditioner.h"
#include "Transformation_actor.h" // ★ 차가운 바람 폼 냉각용
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
        TEXT("SoundWave'/Game/Sound/sound_air_condition.sound_air_condition'"));
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
    {
        if (WindMode == EAirconWindMode::Hot)
            HeatNearbyTemperatureBlocks(DeltaTime);
        else
            CoolNearbyTemperatureBlocks(DeltaTime);
    }

    // ★ WireframeMeshComp stencil 동기화
    if (WireframeMeshComp)
    {
        const float Ratio = FMath::Clamp(Temperature / FMath::Max(MaxStencilTemperature, 1.f), 0.f, 1.f);
        const int32 StencilValue = FMath::RoundToInt(Ratio * 255.f);
        WireframeMeshComp->SetCustomDepthStencilValue(StencilValue);
        WireframeMeshComp->SetRenderCustomDepth(Temperature > 1.f);
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
    }
}

// ★ 차가운 바람: 주변 ATemperature 블럭 냉각 (가열과 동일한 복사 공식, 방향만 반대)
void AAirConditioner::CoolNearbyTemperatureBlocks(float DeltaTime)
{
    if (!HeatSphere) return;

    TArray<AActor*> OverlappingActors;
    HeatSphere->GetOverlappingActors(OverlappingActors);

    static const FName StartHeatingName(TEXT("StartHeating"));

    // 냉각 세기 계산 동안만 복사 공식용 온도로 교체 (계산 후 원복)
    const float SavedTemperature = Temperature;
    Temperature = ColdWindTemperature;

    for (AActor* Actor : OverlappingActors)
    {
        if (!Actor || Actor == this) continue;
        if (Actor->IsA<AAirConditioner>()) continue;

        // ★ 트랜스폼 액터: 폼 온도(자석/금속/고무/구리)를 직접 냉각
        //    (얼음/나무는 폼 온도를 안 쓰므로 영향 없음 → 차가운 바람에 안 녹음)
        if (ATransformation_actor* TransformActor = Cast<ATransformation_actor>(Actor))
        {
            const float ReceivedW = GetReceivedPowerW(
                TransformActor->GetActorLocation(), 1.0f);

            TransformActor->ApplyFormCoolingPower(ReceivedW, DeltaTime);
            continue;
        }

        // 그 외 StartHeating 보유 액터(블루프린트 얼음 등)는 차가운 바람으로 녹지 않음 → 건너뜀
        if (Actor->FindFunction(StartHeatingName)) continue;

        ATemperature* TempBlock = Cast<ATemperature>(Actor);
        if (!TempBlock) continue;

        const float ReceivedW = GetReceivedPowerW(
            TempBlock->GetActorLocation(),
            BlockReceiverAreaM2);

        if (ReceivedW <= 0.f) continue;

        const float DeltaT = (ReceivedW * DeltaTime * HeatSimTimeScale)
                           / (BlockMassKg * BlockSpecificHeatJPerKgK);

        TempBlock->Temperature = FMath::Max(
            TempBlock->Temperature - DeltaT,
            ColdTargetTemperature);
    }

    Temperature = SavedTemperature;
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
    // ★ 차가운 바람 모드일 땐 본체가 뜨거워지지 않음 (스텐실/복사열 방지)
    Temperature = (WindMode == EAirconWindMode::Hot) ? HeatTemperature : 0.f;

    SetSmokeActive(true);   // ★ 연기 ON (+ 0.3초 뒤 사운드)
}

void AAirConditioner::DeactivateAircon()
{
    if (bAlwaysOn) return;
    if (!bIsRunning) return;

    bIsRunning = false;
    Temperature = 0.f;

    SetSmokeActive(false);  // ★ 연기 OFF (+ 사운드 정지)

    // ★ 범위 안 얼음(및 모든 가열 대상)에게 StopHeating 전송
    StopHeatingNearbyBlocks();
}

// ★ 범위 안 가열 대상에게 StopHeating 전송 (끄기/모드 전환 공용)
void AAirConditioner::StopHeatingNearbyBlocks()
{
    if (!HeatSphere) return;

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

// ★ 바람 모드 전환 (발판에서 호출)
void AAirConditioner::SetWindMode(EAirconWindMode NewMode)
{
    if (WindMode == NewMode) return;

    WindMode = NewMode;

    // 작동 중이면 새 모드에 맞게 즉시 상태 갱신
    if (bIsRunning)
    {
        if (WindMode == EAirconWindMode::Cold)
        {
            Temperature = 0.f;
            StopHeatingNearbyBlocks();  // 가열 중이던 얼음 등록 해제
        }
        else
        {
            Temperature = HeatTemperature;
        }
    }

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
        WindMode == EAirconWindMode::Hot
            ? TEXT("[에어컨] 모드 전환 → 뜨거운 바람")
            : TEXT("[에어컨] 모드 전환 → 차가운 바람"));
}

void AAirConditioner::ToggleWindMode()
{
    SetWindMode(WindMode == EAirconWindMode::Hot
        ? EAirconWindMode::Cold
        : EAirconWindMode::Hot);
}