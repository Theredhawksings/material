#include "MoveStage1_2.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AMoveStage1_2::AMoveStage1_2()
{
    PrimaryActorTick.bCanEverTick = true;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AMoveStage1_2::OnOverlapBegin);

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetupAttachment(RootComponent);
}

void AMoveStage1_2::BeginPlay()
{
    Super::BeginPlay();
}

void AMoveStage1_2::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AMoveStage1_2::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor->IsA(ACharacter::StaticClass()))
    {
        GetWorld()->GetTimerManager().SetTimer(
            LevelLoadTimerHandle,
            this,
            &AMoveStage1_2::LoadNextLevel,
            LoadDelay,
            false
        );
    }
}

void AMoveStage1_2::LoadNextLevel()
{
    // 스폰 위치를 옵션 문자열로 전달
    FString Options = FString::Printf(
        TEXT("?SpawnX=%.1f?SpawnY=%.1f?SpawnZ=%.1f?SpawnPitch=%.1f?SpawnYaw=%.1f?SpawnRoll=%.1f"),
        SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z,
        SpawnRotation.Pitch, SpawnRotation.Yaw, SpawnRotation.Roll
    );

    UGameplayStatics::OpenLevel(this, LevelToLoad, true, Options);
}