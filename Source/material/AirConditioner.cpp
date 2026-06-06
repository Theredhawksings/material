#include "AirConditioner.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"

AAirConditioner::AAirConditioner()
{
    PrimaryActorTick.bCanEverTick = true;

    // ── 부모(ATemperature)의 MeshComp 에 에어컨 메시/머터리얼 지정 ──
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

    // ── 얼음 올려놓는 감지 박스 (디버깅 용도) ──
    DetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionBox"));
    DetectionBox->SetupAttachment(MeshComp);
    DetectionBox->SetBoxExtent(FVector(0.005f, 0.005f, 0.005f));
    DetectionBox->SetCollisionProfileName(TEXT("Trigger"));

    // ── 기본은 꺼진 상태 = 온도 0 (가열 안 함) ──
    Temperature = 0.f;
    CoolRate = 0.f;  // 자동 냉각 끄기 (에어컨은 우리가 직접 제어)
}

void AAirConditioner::BeginPlay()
{
    // ATemperature::BeginPlay 가 HeatSphere 세팅 + 가열 시스템 초기화
    Super::BeginPlay();

    DetectionBox->OnComponentBeginOverlap.AddDynamic(this, &AAirConditioner::OnDetectionOverlapBegin);
    DetectionBox->OnComponentEndOverlap.AddDynamic(this, &AAirConditioner::OnDetectionOverlapEnd);

    // ── 시작 시 꺼진 상태 보장 ──
    if (bAlwaysOn)
    {
        ActivateAircon();
    }
    else
    {
        Temperature = 0.f;
        bIsRunning = false;
    }
}

void AAirConditioner::Tick(float DeltaTime)
{
    // ATemperature::Tick 이 Temperature > 0 일 때 주변 블록 자동 가열
    Super::Tick(DeltaTime);

    if (GEngine && DetectionBox)
    {
        TArray<AActor*> OverlappingActors;
        DetectionBox->GetOverlappingActors(OverlappingActors);

        bool bIceDetected = false;
        for (AActor* Actor : OverlappingActors)
        {
            if (Actor && Actor->ActorHasTag(TEXT("Ice")))
            {
                bIceDetected = true;
                break;
            }
        }

        FString DebugMsg = FString::Printf(
            TEXT("=== 에어컨(히터) 상태 ===\n작동: %s\n상시활성화: %s\n현재온도: %.1f\n박스 위 얼음: %s\n감지 액터 수: %d\n"),
            bIsRunning ? TEXT("ON") : TEXT("OFF"),
            bAlwaysOn ? TEXT("ON") : TEXT("OFF"),
            Temperature,
            bIceDetected ? TEXT("있음") : TEXT("없음"),
            OverlappingActors.Num());

        GEngine->AddOnScreenDebugMessage(2, 0.1f, FColor::Cyan, DebugMsg);
    }
}

void AAirConditioner::OnDetectionOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == this) return;

    if (OtherActor->ActorHasTag(TEXT("Ice")))
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
            FString::Printf(TEXT("[감지] 얼음 진입: %s"), *OtherActor->GetName()));
    }
}

void AAirConditioner::OnDetectionOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!OtherActor || OtherActor == this) return;

    if (OtherActor->ActorHasTag(TEXT("Ice")))
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::White,
            FString::Printf(TEXT("[감지] 얼음 퇴장: %s"), *OtherActor->GetName()));
    }
}

void AAirConditioner::ActivateAircon()
{
    if (bIsRunning) return;

    bIsRunning = true;

    // ── 온도를 올리면 ATemperature 가 HeatSphere 범위 안 블록을 자동 가열 ──
    Temperature = HeatTemperature;

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
        TEXT("[에어컨] 작동 시작 (ON) - 가열 시작"));
}

void AAirConditioner::DeactivateAircon()
{
    if (bAlwaysOn) return;
    if (!bIsRunning) return;

    bIsRunning = false;

    // ── 온도 0 = 가열 중지 ──
    Temperature = 0.f;

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::White,
        TEXT("[에어컨] 작동 정지 (OFF) - 가열 중지"));
}