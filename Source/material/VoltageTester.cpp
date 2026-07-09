#include "VoltageTester.h"
#include "Wire.h"
#include "Resistance.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"   // TActorIterator
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

AVoltageTester::AVoltageTester()
{
    PrimaryActorTick.bCanEverTick = true;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    SetRootComponent(MeshComp);
    MeshComp->SetMobility(EComponentMobility::Movable);
    MeshComp->SetCollisionProfileName(TEXT("PhysicsActor"));
    MeshComp->SetGenerateOverlapEvents(true);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> TesterMesh(
        TEXT("/Game/modeling/Object/Voltage_tester/Voltage_tester.Voltage_tester"));
    if (TesterMesh.Succeeded())
        MeshComp->SetStaticMesh(TesterMesh.Object);

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> TesterMat(
        TEXT("/Game/modeling/Object/Voltage_tester/M_Voltage.M_Voltage"));
    if (TesterMat.Succeeded())
        MeshComp->SetMaterial(0, TesterMat.Object);

    // 문 열림 효과음 (고정)
    static ConstructorHelpers::FObjectFinder<USoundBase> DoorSound(
        TEXT("/Game/Sound/sound_door_opening.sound_door_opening"));
    if (DoorSound.Succeeded())
        DoorOpenSound = DoorSound.Object;

    // 목표/현재 전압 표시 텍스트
    StatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
    StatusText->SetupAttachment(MeshComp);
    StatusText->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
    StatusText->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
    StatusText->SetHorizontalAlignment(EHTA_Center);
    StatusText->SetVerticalAlignment(EVRTA_TextCenter);
    StatusText->SetWorldSize(50.f);
    StatusText->SetTextRenderColor(FColor::White);
}

void AVoltageTester::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdateStatusText();
}

void AVoltageTester::UpdateStatusText()
{
    if (!StatusText) return;

    FString Text;
    FColor  Col;
    if (bSolved)
    {
        Text = FString::Printf(TEXT("Target %.1fV\nNow %.2fV\nCLEAR!"), TargetVoltage, MeasuredVoltage);
        Col  = FColor::Green;
    }
    else if (MeasuredResistor)
    {
        Text = FString::Printf(TEXT("Target %.1fV\nNow %.2fV"), TargetVoltage, MeasuredVoltage);
        const bool bMatch = FMath::Abs(MeasuredVoltage - TargetVoltage) <= Tolerance;
        Col  = bMatch ? FColor::Yellow : FColor::White;
    }
    else
    {
        Text = FString::Printf(TEXT("Target %.1fV\nNow ---"), TargetVoltage);
        Col  = FColor::White;
    }

    StatusText->SetText(FText::FromString(Text));
    StatusText->SetTextRenderColor(Col);
}

void AVoltageTester::BeginPlay()
{
    Super::BeginPlay();

    const FVector Dir = OpenDirection.GetSafeNormal();
    if (LeftDoorActor)
        LeftOpenLocation  = LeftDoorActor->GetActorLocation()  + Dir * OpenDistance;
    if (RightDoorActor)
        RightOpenLocation = RightDoorActor->GetActorLocation() - Dir * OpenDistance;

    if (RefreshInterval > 0.f)
        GetWorldTimerManager().SetTimer(RefreshTimerHandle, this,
            &AVoltageTester::RefreshMeasurement, RefreshInterval, true);
}

void AVoltageTester::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(RefreshTimerHandle);
    Super::EndPlay(EndPlayReason);
}

void AVoltageTester::RefreshMeasurement()
{
    MeasuredResistor = nullptr;
    MeasuredVoltage  = 0.f;

    UWorld* World = GetWorld();
    if (!World || !MeasureWire) return;

    // 지정한 전선(MeasureWire)에 연결된 저항 블럭을 찾음
    // → 저항은 솔버한테서 ReceivePower(전압강하, 전류) 를 받아두고 있음
    for (TActorIterator<AResistance> It(World); It; ++It)
    {
        AResistance* R = *It;
        bool bAttached = false;
        for (const TObjectPtr<AWire>& WPtr : R->GetConnectedWiresList())
            if (WPtr.Get() == MeasureWire) { bAttached = true; break; }
        if (!bAttached) continue;

        MeasuredResistor = R;
        MeasuredVoltage  = R->GetEffectiveVoltage();  // 저항 양단 전압강하
        break;
    }

    UpdateStatusText();
}

void AVoltageTester::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ── 정답 판정: 목표 전압 ± 오차 이내가 RequiredHoldTime 동안 유지 ──
    const bool bMatch = MeasuredResistor != nullptr
        && FMath::Abs(MeasuredVoltage - TargetVoltage) <= Tolerance;

    if (!bSolved)
    {
        if (bMatch)
        {
            HoldTimer += DeltaTime;
            if (HoldTimer >= RequiredHoldTime)
            {
                bSolved = true;

                // 문 열림 효과음 (정답 확정 순간 1회 재생)
                if (DoorOpenSound)
                {
                    const FVector SoundPos = LeftDoorActor
                        ? LeftDoorActor->GetActorLocation() : GetActorLocation();
                    UGameplayStatics::PlaySoundAtLocation(this, DoorOpenSound, SoundPos);
                }
            }
        }
        else
        {
            HoldTimer = 0.f;
        }
    }

    // ── 문 열기 (OpenDoor 방식: 좌우 문 반대 방향 보간 이동) ──
    if (bSolved && !bOpened)
    {
        bool bLeftDone  = true;
        bool bRightDone = true;

        if (LeftDoorActor)
        {
            const FVector Current = LeftDoorActor->GetActorLocation();
            const FVector New     = FMath::VInterpConstantTo(Current, LeftOpenLocation, DeltaTime, OpenSpeed);
            LeftDoorActor->SetActorLocation(New);
            bLeftDone = FVector::Dist(New, LeftOpenLocation) < 1.f;
            if (bLeftDone) LeftDoorActor->SetActorLocation(LeftOpenLocation);
        }

        if (RightDoorActor)
        {
            const FVector Current = RightDoorActor->GetActorLocation();
            const FVector New     = FMath::VInterpConstantTo(Current, RightOpenLocation, DeltaTime, OpenSpeed);
            RightDoorActor->SetActorLocation(New);
            bRightDone = FVector::Dist(New, RightOpenLocation) < 1.f;
            if (bRightDone) RightDoorActor->SetActorLocation(RightOpenLocation);
        }

        if (bLeftDone && bRightDone)
            bOpened = true;
    }

#if ENABLE_DRAW_DEBUG
    if (bDrawDebug && GetWorld())
    {
        const FVector Pos = GetActorLocation() + FVector(0.f, 0.f, 80.f);
        FString Text;
        FColor  Col;
        if (bSolved)
        {
            Text = FString::Printf(TEXT("[검측기] %.2fV / 목표 %.1fV  정답!"), MeasuredVoltage, TargetVoltage);
            Col  = FColor::Green;
        }
        else if (MeasuredResistor)
        {
            Text = FString::Printf(TEXT("[검측기] %.2fV / 목표 %.1fV"), MeasuredVoltage, TargetVoltage);
            Col  = bMatch ? FColor::Yellow : FColor::White;
        }
        else if (MeasureWire)
        {
            Text = FString::Printf(TEXT("[검측기] 저항 없음 / 목표 %.1fV"), TargetVoltage);
            Col  = FColor::Silver;
        }
        else
        {
            Text = TEXT("[검측기] 측정 전선 미지정");
            Col  = FColor::Red;
        }
        DrawDebugString(GetWorld(), Pos, Text, nullptr, Col, 0.f, true);
    }
#endif
}
