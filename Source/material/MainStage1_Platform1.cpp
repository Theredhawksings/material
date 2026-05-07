#include "MainStage1_Platform1.h"
#include "Transformation_actor.h"
#include "Engine/Engine.h"

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

    if (TrackedActor && !bActivated)
    {
        if (IsConditionMet())
        {
            bActivated = true;
            bIsOpening = true;

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

            GEngine->AddOnScreenDebugMessage(2, 0.0f, FColor::Yellow,
                FString::Printf(TEXT("Metal:%s | Copper:%s | Electrified:%s"),
                    bMetal  ? TEXT("O") : TEXT("X"),
                    bCopper ? TEXT("O") : TEXT("X"),
                    bElec   ? TEXT("O") : TEXT("X")));
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

    const bool bIsMetal  = TrackedActor->ActorHasTag(FName("Metal"));
    const bool bIsCopper = TrackedActor->ActorHasTag(FName("Copper"));
    const bool bIsConductive = bIsMetal || bIsCopper;

    const bool bHasElectricity = TrackedActor->IsElectrified();

    return bIsConductive && bHasElectricity;
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