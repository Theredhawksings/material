#include "TransformPlatform.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

ATransformPlatform::ATransformPlatform()
    : LeftDoorActor(nullptr)
    , RightDoorActor(nullptr)
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

    PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
    PlatformMesh->SetupAttachment(RootComponent);
}

void ATransformPlatform::BeginPlay()
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

    TriggerBox->OnComponentBeginOverlap.AddDynamic(
        this, &ATransformPlatform::OnOverlapBegin);
    TriggerBox->OnComponentEndOverlap.AddDynamic(
        this, &ATransformPlatform::OnOverlapEnd);
}

void ATransformPlatform::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsOpening && !bIsOpen)
    {
        CurrentTime += DeltaTime * OpenSpeed;
        if (CurrentTime >= 1.0f)
        {
            CurrentTime = 1.0f;
            bIsOpen     = true;
            bIsOpening  = false;

            if (GEngine)
                GEngine->AddOnScreenDebugMessage(-1, 3.0f,
                    FColor::Cyan, TEXT("✅ [PowerKey] 문 열림 완료!"));
        }

        if (LeftDoorActor)
            LeftDoorActor->SetActorLocation(
                FMath::Lerp(LeftStartLocation, LeftTargetLocation, CurrentTime));

        if (RightDoorActor)
            RightDoorActor->SetActorLocation(
                FMath::Lerp(RightStartLocation, RightTargetLocation, CurrentTime));
    }

#if ENABLE_DRAW_DEBUG
    if (bDebugDraw)
    {
        const FColor BoxColor = bActivated ? FColor::Cyan : FColor::Orange;
        DrawDebugBox(GetWorld(),
            TriggerBox->GetComponentLocation(),
            TriggerBox->GetScaledBoxExtent(),
            TriggerBox->GetComponentQuat(),
            BoxColor, false, -1.f, 0, 2.f);

        DrawDebugString(GetWorld(),
            GetActorLocation() + FVector(0, 0, 80.f),
            FString::Printf(TEXT("[TransformPlatform]\n상태: %s\n태그: %s"),
                bActivated ? TEXT("활성화됨") : TEXT("대기중"),
                *RequiredTag.ToString()),
            nullptr,
            bActivated ? FColor::Cyan : FColor::Yellow,
            0.f, true);
    }
#endif
}

void ATransformPlatform::OnOverlapBegin(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!OtherActor || bActivated) return;

    if (!OtherActor->ActorHasTag(RequiredTag)) return;

    bActivated  = true;
    bIsOpening  = true;
    CurrentTime = 0.0f;

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

    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 3.0f,
            FColor::Cyan, TEXT("✅ [PowerKey] 감지! 문이 열립니다"));

    UE_LOG(LogTemp, Log, TEXT("TransformPlatform: %s 감지 → 문 열기"),
        *OtherActor->GetName());
}

void ATransformPlatform::OnOverlapEnd(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32)
{
    // 한 번 열리면 닫히지 않음
}