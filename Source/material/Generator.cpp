#include "Generator.h"
#include "Magnet.h"
#include "Wire.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "UObject/ConstructorHelpers.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

AGenerator::AGenerator()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // 1) 고정 본체 컴포넌트 생성 및 에셋 로드
    GeneratorBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GeneratorBody"));
    GeneratorBody->SetupAttachment(Root);
    GeneratorBody->SetRelativeScale3D(FVector(1.f, 1.f, 1.f)); // 👈 본체 스케일 1, 1, 1 강제 고정!
    GeneratorBody->SetSimulatePhysics(false);
    GeneratorBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMeshAsset(
        TEXT("/Script/Engine.StaticMesh'/Game/modeling/Object/Generator_Rotor/Generator_Rotor_body.Generator_Rotor_body'"));
    if (BodyMeshAsset.Succeeded())
    {
        GeneratorBody->SetStaticMesh(BodyMeshAsset.Object);
    }

    // 2) 회전 코일 컴포넌트 생성 및 에셋 로드 (원점 부착)
    CoilBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoilBody"));
    CoilBody->SetupAttachment(Root);
    CoilBody->SetRelativeScale3D(FVector(1.f, 1.f, 1.f)); // 👈 코일 스케일 1, 1, 1 강제 고정!
    CoilBody->SetSimulatePhysics(false);
    CoilBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CoilMeshAsset(
        TEXT("/Script/Engine.StaticMesh'/Game/modeling/Object/Generator_Rotor/Copper_Coil_Body.Copper_Coil_Body'"));
    if (CoilMeshAsset.Succeeded())
    {
        CoilBody->SetStaticMesh(CoilMeshAsset.Object);
    }

    // 3) 머티리얼 에셋 로드 및 적용
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialAsset(
        TEXT("/Script/Engine.Material'/Game/modeling/Object/Generator_Rotor/M_Generator_Rotor.M_Generator_Rotor'"));
    if (MaterialAsset.Succeeded())
    {
        GeneratorBody->SetMaterial(0, MaterialAsset.Object);
        CoilBody->SetMaterial(0, MaterialAsset.Object);
    }

    // 4) 출력 박스 설정
    OutputBox = CreateDefaultSubobject<UBoxComponent>(TEXT("OutputBox"));
    OutputBox->SetupAttachment(Root);
    OutputBox->SetRelativeScale3D(FVector(1.f, 1.f, 1.f)); // 👈 혹시 몰라서 박스도 스케일 1, 1, 1 고정!
    OutputBox->SetBoxExtent(FVector(40.f, 40.f, 40.f));
    OutputBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OutputBox->SetCollisionResponseToAllChannels(ECR_Ignore);

    // 사운드 로드
    static ConstructorHelpers::FObjectFinder<USoundBase> TurnOnAsset(
        TEXT("/Script/Engine.SoundWave'/Game/Sound/sound_generator_turning_on.sound_generator_turning_on'"));
    if (TurnOnAsset.Succeeded())
        TurningOnSound = TurnOnAsset.Object;

    static ConstructorHelpers::FObjectFinder<USoundBase> TurnOffAsset(
        TEXT("/Script/Engine.SoundWave'/Game/Sound/sound_generator_turning_off.sound_generator_turning_off'"));
    if (TurnOffAsset.Succeeded())
        TurningOffSound = TurnOffAsset.Object;
}

void AGenerator::BeginPlay()
{
    Super::BeginPlay();
    DetectMagnets();
}

void AGenerator::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    MagnetScanAccumulator += DeltaTime;
    if (MagnetScanInterval <= 0.f || MagnetScanAccumulator >= MagnetScanInterval)
    {
        MagnetScanAccumulator = 0.f;
        DetectMagnets();
    }

    UpdateEMF(DeltaTime);
    UpdateCircuit();
    UpdateGeneratorSound();

    UpdateThermal(DeltaTime);

#if ENABLE_DRAW_DEBUG
    if (!bDebugDraw) return;

    const FVector MyLoc    = GetActorLocation();
    const FVector BoxLoc   = OutputBox->GetComponentLocation();
    const bool bGenerating = (FMath::Abs(CurrentEMF) >= MinEMFThreshold);
    const bool bOutputOn   = bCoilOutputEnabled && bGenerating && bCurrentPositive;

    DrawDebugSphere(GetWorld(), MyLoc,
        MagnetDetectRadius, 16, FColor::Yellow, false, -1.f, 0, 1.f);

    DrawDebugBox(GetWorld(), BoxLoc,
        OutputBox->GetScaledBoxExtent(), OutputBox->GetComponentQuat(),
        bOutputOn ? FColor::Cyan : FColor(100, 100, 100),
        false, -1.f, 0, 2.f);

    for (AMagnet* M : NorthMagnets)
        if (M) DrawDebugSphere(GetWorld(), M->GetActorLocation(),
            40.f, 8, FColor::Red, false, -1.f, 0, 3.f);
    for (AMagnet* M : SouthMagnets)
        if (M) DrawDebugSphere(GetWorld(), M->GetActorLocation(),
            40.f, 8, FColor::Blue, false, -1.f, 0, 3.f);

    DrawDebugString(GetWorld(), MyLoc + FVector(0, 0, 100.f),
        FString::Printf(TEXT(
            "[Generator]\n"
            "N극: %d개 / S극: %d개\n"
            "유효쌍: %d / 불균형: %.0f%%\n"
            "회전속도: %.1f deg/s\n"
            "회전각: %.1f\n"
            "EMF: %.2f V\n"
            "전류방향: %s\n"
            "전기출력: %s\n"
            "박스내Wire: %d개"),
            NorthMagnets.Num(), SouthMagnets.Num(),
            EffectivePairs, ImbalanceRatio * 100.f,
            CurrentRotationSpeed,
            RotationAngle,
            CurrentEMF,
            bCurrentPositive ? TEXT("정방향(+)") : TEXT("역방향(-)"),
            bOutputOn ? TEXT("ON") : TEXT("OFF"),
            BoxPoweredWires.Num()),
        nullptr, FColor::Green, 0.f, true);
#endif
}

void AGenerator::DetectMagnets()
{
    NorthMagnets.Reset(); 
    SouthMagnets.Reset();

    const FVector MyLoc    = GetActorLocation();
    const float   RadiusSq = FMath::Square(MagnetDetectRadius);

    TArray<AActor*> FoundMagnets;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMagnet::StaticClass(), FoundMagnets);

    for (AActor* Actor : FoundMagnets)
    {
        AMagnet* Magnet = Cast<AMagnet>(Actor);
        if (!Magnet || Magnet->IsDemagnetized()) continue;

        if (FVector::DistSquared(MyLoc, Magnet->GetActorLocation()) > RadiusSq) continue;

        if (Magnet->IsNorthPole()) NorthMagnets.Add(Magnet);
        else                       SouthMagnets.Add(Magnet);
    }
}

void AGenerator::UpdateEMF(float DeltaTime)
{
    if (!bGeneratorActive || (NorthMagnets.Num() == 0 && SouthMagnets.Num() == 0))
    {
        CurrentEMF           = 0.f;
        RotationAngle        = 0.f;
        EffectivePairs       = 0;
        ImbalanceRatio       = 0.f;
        CurrentRotationSpeed = BaseRotationSpeed;
        
        // 발전기가 정지하면 코일 메시의 회전을 초기화
        CoilBody->SetRelativeRotation(FRotator::ZeroRotator);
        return;
    }

    const FVector MyLoc = GetActorLocation();

    const int32 NCount = NorthMagnets.Num();
    const int32 SCount = SouthMagnets.Num();
    EffectivePairs = FMath::Min(NCount, SCount);

    ImbalanceRatio = (NCount + SCount > 0)
        ? (float)FMath::Abs(NCount - SCount) / (float)(NCount + SCount)
        : 0.f;

    if (EffectivePairs < MinRequiredPairs)
    {
        CurrentEMF = 0.f;
        return;
    }

    CurrentRotationSpeed = BaseRotationSpeed;

    FVector NorthCentroid = FVector::ZeroVector;
    for (AMagnet* M : NorthMagnets)
        if (M) NorthCentroid += M->GetActorLocation();
    NorthCentroid /= (float)NorthMagnets.Num();

    const FVector ToNorth = (NorthCentroid - MyLoc).GetSafeNormal();
    const float Dot    = FVector::DotProduct(ToNorth, GetActorRightVector());
    const float RotDir = (Dot > 0.f) ? 1.f : -1.f;

    RotationAngle += RotDir * CurrentRotationSpeed * DeltaTime;
    if (RotationAngle >= 360.f)  RotationAngle -= 360.f;
    if (RotationAngle <= -360.f) RotationAngle += 360.f;

    // 전체 본체가 아닌 코일 메시만 회전하도록 변경
    CoilBody->SetRelativeRotation((SpinAxisMask * RotationAngle).Quaternion());

    struct FMagnetDistSq { AMagnet* Magnet; float DistSq; };
    TArray<FMagnetDistSq, TInlineAllocator<8>> SortedN, SortedS;
    SortedN.Reserve(NCount);
    SortedS.Reserve(SCount);

    for (const TObjectPtr<AMagnet>& M : NorthMagnets)
        if (M) SortedN.Add({ M.Get(), (float)FVector::DistSquared(MyLoc, M->GetActorLocation()) });
    for (const TObjectPtr<AMagnet>& M : SouthMagnets)
        if (M) SortedS.Add({ M.Get(), (float)FVector::DistSquared(MyLoc, M->GetActorLocation()) });

    SortedN.Sort([](const FMagnetDistSq& A, const FMagnetDistSq& B){ return A.DistSq < B.DistSq; });
    SortedS.Sort([](const FMagnetDistSq& A, const FMagnetDistSq& B){ return A.DistSq < B.DistSq; });

    const int32 PairLoop = FMath::Min(EffectivePairs, FMath::Min(SortedN.Num(), SortedS.Num()));

    float TotalB = 0.f;
    for (int32 i = 0; i < PairLoop; ++i)
    {
        AMagnet* MN = SortedN[i].Magnet;
        AMagnet* MS = SortedS[i].Magnet;

        const float DistN = FMath::Max(FMath::Sqrt(SortedN[i].DistSq), 1.f);
        const float DistS = FMath::Max(FMath::Sqrt(SortedS[i].DistSq), 1.f);

        const float BN = (MN->GetStrength() / 1000000.f)
                       * FMath::Pow(MN->GetReferenceDistance() / DistN, MN->GetDecayExponent());
        const float BS = (MS->GetStrength() / 1000000.f)
                       * FMath::Pow(MS->GetReferenceDistance() / DistS, MS->GetDecayExponent());
        TotalB += (BN + BS) * 0.5f;
    }

    const float AngleRad        = FMath::DegreesToRadians(RotationAngle);
    const float AngularVelocity = FMath::DegreesToRadians(CurrentRotationSpeed);
    const float CoilRadius      = CoilRadiusCM / 100.f;
    const float CoilArea        = PI * CoilRadius * CoilRadius;

    float EMF = CoilWindings * TotalB * CoilArea
              * AngularVelocity
              * FMath::Sin(AngleRad * (float)EffectivePairs);

    if (ImbalanceRatio > 0.f && ImbalancePenaltyScale > 0.f)
    {
        ImbalanceNoiseTime += DeltaTime;
        const float Noise = FMath::Sin(ImbalanceNoiseTime * 17.3f)
                          * FMath::Cos(ImbalanceNoiseTime * 11.7f);
        const float PenaltyFactor = 1.f
            - (ImbalanceRatio * ImbalancePenaltyScale * 0.5f * (1.f + Noise));
        EMF *= FMath::Max(PenaltyFactor, 0.f);
    }
    else
    {
        ImbalanceNoiseTime = 0.f;
    }

    CurrentEMF       = EMF;
    bCurrentPositive = (CurrentEMF >= 0.f);
}

void AGenerator::UpdateCircuit()
{
    const float AbsEMF      = FMath::Abs(CurrentEMF);
    const bool  bGenerating = (AbsEMF >= MinEMFThreshold);

    // ── 1) AssignedWires: 부착/분리 로직을 제외하고 전력 공급만 처리 ──
    for (TObjectPtr<AWire>& WirePtr : AssignedWires)
    {
        AWire* Wire = WirePtr.Get();
        if (!Wire) continue;

        if (bGenerating && bCurrentPositive)
        {
            Wire->SetBatterySource(true);
            Wire->SetBatteryVoltage(AbsEMF);
            Wire->SetOpenCircuitHeating(true);   // 개방 회로여도 발열
            Wire->SetPowered(true);
        }
        else
        {
            Wire->SetBatterySource(false);
            Wire->SetBatteryVoltage(0.f);
            Wire->SetOpenCircuitHeating(false);
            Wire->SetPowered(false);
        }
    }

    // ── 2) OutputBox: 이전 프레임 전선 전원 끊기 ──
    for (TObjectPtr<AWire>& WirePtr : BoxPoweredWires)
    {
        AWire* Wire = WirePtr.Get();
        if (!Wire) continue;
        Wire->SetBatterySource(false);
        Wire->SetBatteryVoltage(0.f);
        Wire->SetOpenCircuitHeating(false);
        Wire->SetPowered(false);
    }
    BoxPoweredWires.Reset(); 

    if (!bCoilOutputEnabled || !bGenerating || !bCurrentPositive) return;

    // ── 3) OutputBox 안 전선 탐지 + 전기 중계 ──
    const FTransform BoxXform  = OutputBox->GetComponentTransform();
    const FVector    BoxCenter = BoxXform.GetLocation();
    const FQuat      BoxRot    = BoxXform.GetRotation();
    const FVector    BoxExtent = OutputBox->GetScaledBoxExtent();

    TArray<FOverlapResult> Hits;
    FCollisionQueryParams QParams(SCENE_QUERY_STAT(GeneratorOutputSense), false);
    QParams.AddIgnoredActor(this);

    GetWorld()->OverlapMultiByObjectType(
        Hits, BoxCenter, BoxRot,
        FCollisionObjectQueryParams::AllObjects,
        FCollisionShape::MakeBox(BoxExtent), QParams);

    for (const FOverlapResult& H : Hits)
    {
        AWire* Wire = Cast<AWire>(H.GetActor());
        if (!Wire) continue;

        const FVector StartPt = Wire->GetStartPointLocation();
        const FVector LocalPt = BoxXform.InverseTransformPosition(StartPt);
        if (FMath::Abs(LocalPt.X) > BoxExtent.X ||
            FMath::Abs(LocalPt.Y) > BoxExtent.Y ||
            FMath::Abs(LocalPt.Z) > BoxExtent.Z) continue;

        bool bAlreadyAssigned = false;
        for (const TObjectPtr<AWire>& AW : AssignedWires)
            if (AW.Get() == Wire) { bAlreadyAssigned = true; break; }
        if (bAlreadyAssigned) continue;

        Wire->SetBatterySource(true);
        Wire->SetBatteryVoltage(AbsEMF);
        Wire->SetOpenCircuitHeating(true);   // 개방 회로여도 발열
        Wire->SetPowered(true);

        BoxPoweredWires.Add(Wire);
    }
}

void AGenerator::UpdateGeneratorSound()
{
    const bool bRunning = bGeneratorActive && (EffectivePairs >= MinRequiredPairs);

    if (bRunning != bWasRunning)
    {
        bWasRunning = bRunning;

        GetWorld()->GetTimerManager().ClearTimer(GenSoundTimerHandle);

        if (ActiveGenAudio)
        {
            ActiveGenAudio->Stop();
            ActiveGenAudio = nullptr;
        }

        if (!bRunning)
        {
            if (TurningOffSound)
                UGameplayStatics::PlaySoundAtLocation(
                    this, TurningOffSound, GetActorLocation(),
                    1.f, 1.f, 0.f,
                    GeneratorSoundAttenuation);
            return;
        }
    }

    if (bRunning && TurningOnSound)
    {
        if (!ActiveGenAudio || !ActiveGenAudio->IsPlaying())
        {
            ActiveGenAudio = UGameplayStatics::SpawnSoundAttached(
                TurningOnSound, GetRootComponent(),
                NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset,
                false, 1.f, 1.f, 0.f,
                GeneratorSoundAttenuation);
        }
    }
}

void AGenerator::UpdateThermal(float DeltaTime)
{
    // EMF가 최소치 이상 발생 중이면 발전기가 작동하는 것으로 간주
    const bool bGenerating = (FMath::Abs(CurrentEMF) >= MinEMFThreshold);

    if (bGenerating)
    {
        // 작동 중일 때는 스텐실(온도) 값을 즉시 255로 세팅
        CurrentThermalValue = 255.f;
    }
    else
    {
        // 멈췄을 때는 CooldownRate에 따라 서서히 식음
        if (CurrentThermalValue > 0.f)
        {
            CurrentThermalValue -= ThermalCooldownRate * DeltaTime;
            if (CurrentThermalValue < 0.f)
            {
                CurrentThermalValue = 0.f;
            }
        }
    }

    // float 값을 정수(0~255)로 변환
    const int32 StencilValue = FMath::RoundToInt(CurrentThermalValue);

    // 스텐실 값이 1 이상일 때만 CustomDepth를 켜서 성능 확보
    if (StencilValue > 0)
    {
        GeneratorBody->SetRenderCustomDepth(true);
        GeneratorBody->SetCustomDepthStencilValue(StencilValue);
        
        CoilBody->SetRenderCustomDepth(true);
        CoilBody->SetCustomDepthStencilValue(StencilValue);
    }
    else
    {
        // 다 식었으면(0) 렌더링 부하를 없애기 위해 CustomDepth 끄기
        GeneratorBody->SetRenderCustomDepth(false);
        CoilBody->SetRenderCustomDepth(false);
    }
}