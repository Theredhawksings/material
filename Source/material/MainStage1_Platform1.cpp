#include "MainStage1_Platform1.h"
#include "Transformation_actor.h"
#include "Engine/Engine.h"
#include "Components/TextRenderComponent.h"
#include "UObject/ConstructorHelpers.h"   // ★ 추가
#include "Sound/SoundBase.h"              // ★ 추가
#include "Kismet/GameplayStatics.h"       // ★ 추가

AMainStage1_Platform1::AMainStage1_Platform1()
    : TrackedActor(nullptr)
    , bActivated(false)
    , bIsOpening(false)
    , bIsOpen(false)
    , CurrentTime(0.0f)
{
    PrimaryActorTick.bCanEverTick = true;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f));
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AMainStage1_Platform1::OnOverlapBegin);
    TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AMainStage1_Platform1::OnOverlapEnd);

    PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
    PlatformMesh->SetupAttachment(RootComponent);

    // ★ 문 열리는 효과음 로드
    static ConstructorHelpers::FObjectFinder<USoundBase> DoorOpenAsset(
        TEXT("/Script/Engine.SoundWave'/Game/Sound/sound_door_opening.sound_door_opening'"));
    if (DoorOpenAsset.Succeeded())
        DoorOpenSound = DoorOpenAsset.Object;
}

void AMainStage1_Platform1::BeginPlay()
{
    Super::BeginPlay();

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

    FTimerHandle InitTimer;
    GetWorld()->GetTimerManager().SetTimer(InitTimer, [this]()
    {
        TArray<AActor*> OverlappingActors;
        TriggerBox->GetOverlappingActors(OverlappingActors, ATransformation_actor::StaticClass());
        for (AActor* Actor : OverlappingActors)
        {
            if (ATransformation_actor* T = Cast<ATransformation_actor>(Actor))
            {
                TrackedActor = T;
                break;
            }
        }
    }, 0.1f, false);
}

void AMainStage1_Platform1::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ★ 칠판 업데이트
    if (VoltageDisplay)
    {
        FString Txt;
        if (TrackedActor)
        {
            const float V = TrackedActor->GetEffectiveVoltage();
            Txt = FString::Printf(TEXT("Now : %.1fV\nGoal: %.1fV"), V, RequiredVoltage);
        }
        else
        {
            Txt = FString::Printf(TEXT("현재: 0.0V\n목표: %.1fV"), RequiredVoltage);
        }
        VoltageDisplay->GetTextRender()->SetText(FText::FromString(Txt));
    }

    if (TrackedActor && !bActivated)
    {
        if (IsConditionMet())
        {
            bActivated = true;
            bIsOpening = true;

            // ★ 문 열리는 효과음 (조건 충족 시 딱 한 번만 재생)
            if (DoorOpenSound)
                UGameplayStatics::PlaySoundAtLocation(this, DoorOpenSound, GetActorLocation());

            if (GEngine)
                GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("⚡ 전기 감지! 문이 열립니다"));

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
        }
        else if (GEngine)
        {
            const bool bMetal  = TrackedActor->ActorHasTag(FName("Metal"));
            const bool bCopper = TrackedActor->ActorHasTag(FName("Copper"));
            const bool bElec   = TrackedActor->IsElectrified();
            const float V      = TrackedActor->GetEffectiveVoltage();

            GEngine->AddOnScreenDebugMessage(2, 0.0f, FColor::Yellow,
                FString::Printf(TEXT("Metal:%s | Copper:%s | Electrified:%s | V:%.1f / 목표:%.1f"),
                    bMetal  ? TEXT("O") : TEXT("X"),
                    bCopper ? TEXT("O") : TEXT("X"),
                    bElec   ? TEXT("O") : TEXT("X"),
                    V, RequiredVoltage));
        }
    }

    if (bIsOpening && !bIsOpen)
    {
        CurrentTime += DeltaTime * OpenSpeed;
        if (CurrentTime >= 1.0f)
        {
            CurrentTime = 1.0f;
            bIsOpen     = true;
            bIsOpening  = false;
        }

        if (LeftDoorActor)
            LeftDoorActor->SetActorLocation(
                FMath::Lerp(LeftStartLocation, LeftTargetLocation, CurrentTime));

        if (RightDoorActor)
            RightDoorActor->SetActorLocation(
                FMath::Lerp(RightStartLocation, RightTargetLocation, CurrentTime));
    }
}

bool AMainStage1_Platform1::IsConditionMet() const
{
    if (!TrackedActor) return false;

    const float V = TrackedActor->GetEffectiveVoltage();

    return FMath::IsNearlyEqual(V, RequiredVoltage, 0.1f);
}

void AMainStage1_Platform1::OnOverlapBegin(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!OtherActor) return;
    if (ATransformation_actor* T = Cast<ATransformation_actor>(OtherActor))
        TrackedActor = T;
}

void AMainStage1_Platform1::OnOverlapEnd(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32)
{
    if (OtherActor == TrackedActor)
        TrackedActor = nullptr;
}