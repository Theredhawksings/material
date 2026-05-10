#include "MainStage1_Platform2.h"
#include "Transformation_actor.h"
#include "Engine/Engine.h"

AMainStage1_Platform2::AMainStage1_Platform2()
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
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AMainStage1_Platform2::OnOverlapBegin);
    TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AMainStage1_Platform2::OnOverlapEnd);

    PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
    PlatformMesh->SetupAttachment(RootComponent);
}

void AMainStage1_Platform2::BeginPlay()
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

void AMainStage1_Platform2::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (TrackedActor && !bActivated)
    {
        if (IsConditionMet())
        {
            bActivated = true;
            bIsOpening = true;

            if (GEngine)
                GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("✅ 태그 감지! 문이 열립니다"));

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
            GEngine->AddOnScreenDebugMessage(3, 0.0f, FColor::Yellow,
                FString::Printf(TEXT("Tag [%s] 감지 대기중..."), *RequiredComponentTag.ToString()));
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

bool AMainStage1_Platform2::IsConditionMet() const
{
    if (!TrackedActor) return false;

    TArray<UActorComponent*> Components;
    TrackedActor->GetComponents(Components);

    for (UActorComponent* Comp : Components)
    {
        if (Comp->ComponentTags.Contains(RequiredComponentTag))
            return true;
    }

    return false;
}

void AMainStage1_Platform2::OnOverlapBegin(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!OtherActor) return;
    if (ATransformation_actor* T = Cast<ATransformation_actor>(OtherActor))
        TrackedActor = T;
}

void AMainStage1_Platform2::OnOverlapEnd(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32)
{
    if (OtherActor == TrackedActor)
        TrackedActor = nullptr;
}