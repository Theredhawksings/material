#include "OpenDoor.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "UObject/ConstructorHelpers.h"   // ★ 추가
#include "Sound/SoundBase.h"              // ★ 추가
#include "Kismet/GameplayStatics.h"       // ★ 추가

AOpenDoor::AOpenDoor()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    DetectBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectBox"));
    DetectBox->SetupAttachment(Root);
    DetectBox->SetBoxExtent(FVector(60.f, 60.f, 20.f));
    DetectBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DetectBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    DetectBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    // ★ 문 열리는 효과음 로드
    static ConstructorHelpers::FObjectFinder<USoundBase> DoorOpenAsset(
        TEXT("/Script/Engine.SoundWave'/Game/Sound/sound_door_opening.sound_door_opening'"));
    if (DoorOpenAsset.Succeeded())
        DoorOpenSound = DoorOpenAsset.Object;
}

void AOpenDoor::BeginPlay()
{
    Super::BeginPlay();

    DetectBox->OnComponentBeginOverlap.AddDynamic(this, &AOpenDoor::OnBoxBeginOverlap);
    DetectBox->OnComponentEndOverlap.AddDynamic(this,   &AOpenDoor::OnBoxEndOverlap);

    if (LeftDoorActor)
    {
        LeftClosedLocation = LeftDoorActor->GetActorLocation();
        LeftOpenLocation   = LeftClosedLocation + FVector(0.f, OpenDistance, 0.f);
    }

    if (RightDoorActor)
    {
        RightClosedLocation = RightDoorActor->GetActorLocation();
        RightOpenLocation   = RightClosedLocation + FVector(0.f, -OpenDistance, 0.f);
    }
}

void AOpenDoor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bOpened || !bOpening) return;

    bool bLeftDone  = true;
    bool bRightDone = true;

    if (LeftDoorActor)
    {
        const FVector Current = LeftDoorActor->GetActorLocation();
        const FVector New     = FMath::VInterpConstantTo(
            Current, LeftOpenLocation, DeltaTime, OpenSpeed);
        LeftDoorActor->SetActorLocation(New);

        bLeftDone = FVector::Dist(New, LeftOpenLocation) < 1.f;
        if (bLeftDone)
            LeftDoorActor->SetActorLocation(LeftOpenLocation);
    }

    if (RightDoorActor)
    {
        const FVector Current = RightDoorActor->GetActorLocation();
        const FVector New     = FMath::VInterpConstantTo(
            Current, RightOpenLocation, DeltaTime, OpenSpeed);
        RightDoorActor->SetActorLocation(New);

        bRightDone = FVector::Dist(New, RightOpenLocation) < 1.f;
        if (bRightDone)
            RightDoorActor->SetActorLocation(RightOpenLocation);
    }

    if (bLeftDone && bRightDone)
    {
        bOpened  = true;
        bOpening = false;
    }
}

void AOpenDoor::OnBoxBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!Cast<ACharacter>(OtherActor)) return;

    const bool bWasEmpty = OverlappingPlayers.IsEmpty();
    OverlappingPlayers.Add(OtherActor);

    if (bWasEmpty && !bOpened)
    {
        bOpening = true;

        // ★ 문 열리기 시작할 때 효과음 (한 번만 재생)
        if (DoorOpenSound)
            UGameplayStatics::PlaySoundAtLocation(this, DoorOpenSound, GetActorLocation());
    }
}

void AOpenDoor::OnBoxEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32)
{
    if (!Cast<ACharacter>(OtherActor)) return;
    OverlappingPlayers.Remove(OtherActor);
}