#include "IronSpawner.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"

AIronSpawner::AIronSpawner()
{
    PrimaryActorTick.bCanEverTick = true;

    // 루트
    DefaultRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRootComp"));
    SetRootComponent(DefaultRoot);

    // ★ 스포너 본체 메시 (루트에 부착)
    SpawnerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpawnerMesh"));
    SpawnerMesh->SetupAttachment(RootComponent);
    SpawnerMesh->SetSimulatePhysics(false);
    SpawnerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 충돌 없음 (장식용)

    // 소환 위치 (SpawnerMesh에 부착 → 메시 위에서 철이 나옴)
    SpawnLocationComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnLocationComp"));
    SpawnLocationComponent->SetupAttachment(SpawnerMesh);
    // ★ 기본 위치: 메시 위쪽 (에디터에서 조정 가능)
    SpawnLocationComponent->SetRelativeLocation(FVector(0.f, 0.f, 100.f));

    // 삭제 구역 박스 (루트에 부착)
    DestructionZoneComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("DestructionZoneComp"));
    DestructionZoneComponent->SetupAttachment(RootComponent);
    DestructionZoneComponent->SetBoxExtent(FVector(200.0f, 200.0f, 50.0f));
    DestructionZoneComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DestructionZoneComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
    DestructionZoneComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);

    SpawnInterval = 4.0f;
    IronLifeTime  = 3.5f;
    IronClass     = ATransformation_actor::StaticClass();
}

void AIronSpawner::BeginPlay()
{
    Super::BeginPlay();

    DestructionZoneComponent->OnComponentBeginOverlap.AddDynamic(
        this, &AIronSpawner::OnZoneBeginOverlap);
    DestructionZoneComponent->OnComponentEndOverlap.AddDynamic(
        this, &AIronSpawner::OnZoneEndOverlap);

    GetWorldTimerManager().SetTimer(
        SpawnTimerHandle,
        this,
        &AIronSpawner::SpawnIron,
        SpawnInterval,
        true);
}

void AIronSpawner::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    CheckIronLifeTime(DeltaTime);
}

void AIronSpawner::SpawnIron()
{
    UWorld* World = GetWorld();
    if (!World || !IronClass) return;

    FVector SpawnLocation   = SpawnLocationComponent->GetComponentLocation();
    FRotator SpawnRotation  = SpawnLocationComponent->GetComponentRotation();

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ATransformation_actor* NewIron = World->SpawnActor<ATransformation_actor>(
        IronClass, SpawnLocation, SpawnRotation, SpawnParams);

    if (NewIron)
    {
        NewIron->SetForm(EBlockForm::Metal);
        NewIron->SetActorScale3D(FVector(0.6f, 0.6f, 0.6f));
        if (NewIron->MeshComp)
            NewIron->MeshComp->SetWorldScale3D(FVector(0.6f, 0.6f, 0.6f));

        FIronSpawnData NewData;
        NewData.IronActor  = NewIron;
        NewData.TimeInZone = 0.0f;
        NewData.bIsInZone  = false;
        SpawnedIronList.Add(NewData);
    }
}

void AIronSpawner::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                      bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor) return;

    UE_LOG(LogTemp, Warning, TEXT("삭제 구역 진입: %s"), *OtherActor->GetName());

    for (int32 i = 0; i < SpawnedIronList.Num(); ++i)
    {
        if (SpawnedIronList[i].IronActor == OtherActor)
        {
            SpawnedIronList[i].bIsInZone = true;
            break;
        }
    }
}

void AIronSpawner::OnZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!OtherActor) return;

    UE_LOG(LogTemp, Warning, TEXT("삭제 구역 탈출: %s"), *OtherActor->GetName());

    for (int32 i = 0; i < SpawnedIronList.Num(); ++i)
    {
        if (SpawnedIronList[i].IronActor == OtherActor)
        {
            SpawnedIronList[i].bIsInZone  = false;
            SpawnedIronList[i].TimeInZone = 0.0f;
            break;
        }
    }
}

void AIronSpawner::CheckIronLifeTime(float DeltaTime)
{
    for (int32 i = SpawnedIronList.Num() - 1; i >= 0; --i)
    {
        ATransformation_actor* Iron = SpawnedIronList[i].IronActor;

        if (!IsValid(Iron))
        {
            SpawnedIronList.RemoveAt(i);
            continue;
        }

        if (SpawnedIronList[i].bIsInZone)
        {
            SpawnedIronList[i].TimeInZone += DeltaTime;

            if (SpawnedIronList[i].TimeInZone >= IronLifeTime)
            {
                UE_LOG(LogTemp, Error, TEXT("방치 시간 초과 → 철 파괴: %s"), *Iron->GetName());
                Iron->Destroy();
                SpawnedIronList.RemoveAt(i);
            }
        }
    }
}

void AIronSpawner::OnIronConsumed(AActor* ConsumedIron)
{
    if (!ConsumedIron) return;

    for (int32 i = 0; i < SpawnedIronList.Num(); ++i)
    {
        if (SpawnedIronList[i].IronActor == ConsumedIron)
        {
            ConsumedIron->Destroy();
            SpawnedIronList.RemoveAt(i);
            SpawnIron();
            break;
        }
    }
}