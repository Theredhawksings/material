// Fill out your copyright notice in the Description page of Project Settings.


#include "IronSpawner.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"

AIronSpawner::AIronSpawner()
{
    PrimaryActorTick.bCanEverTick = true;

    // 진짜 기준이 될 루트 컴포넌트 생성
    DefaultRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRootComp"));
    SetRootComponent(DefaultRoot);

    // [소환하는 곳]을 루트에 부착 (에디터에서 따로 움직일 수 있음)
    SpawnLocationComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnLocationComp"));
    SpawnLocationComponent->SetupAttachment(RootComponent);

    // [삭제되는 곳]을 루트에 부착 (소환 구역과 별개로 따로 움직일 수 있음)
    DestructionZoneComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("DestructionZoneComp"));
    DestructionZoneComponent->SetupAttachment(RootComponent);
    
    // 박스 기본 크기 설정 (원하는 대로 에디터에서 수정 가능)
    DestructionZoneComponent->SetBoxExtent(FVector(200.0f, 200.0f, 50.0f));
    
    // ★ 콜리전 설정을 명확하게 코드로 강제 (Overlap 관련 버그 원천 차단)
    DestructionZoneComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DestructionZoneComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
    DestructionZoneComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);

    // 기본 시간 세팅 (생산 4초, 방치 3.5초)
    SpawnInterval = 4.0f; 
    IronLifeTime = 3.5f;  
    IronClass = ATransformation_actor::StaticClass();
}

void AIronSpawner::BeginPlay()
{
    Super::BeginPlay();
    
    // 이벤트 바인딩
    DestructionZoneComponent->OnComponentBeginOverlap.AddDynamic(this, &AIronSpawner::OnZoneBeginOverlap);
    DestructionZoneComponent->OnComponentEndOverlap.AddDynamic(this, &AIronSpawner::OnZoneEndOverlap);

    // 스폰 타이머 작동
    GetWorldTimerManager().SetTimer(
        SpawnTimerHandle, 
        this, 
        &AIronSpawner::SpawnIron, 
        SpawnInterval, 
        true
    );
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

    // 소환하는 곳(SpawnLocationComponent)의 위치를 기준으로 스폰
    FVector SpawnLocation = SpawnLocationComponent->GetComponentLocation();
    FRotator SpawnRotation = SpawnLocationComponent->GetComponentRotation();

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ATransformation_actor* NewIron = World->SpawnActor<ATransformation_actor>(IronClass, SpawnLocation, SpawnRotation, SpawnParams);

    if (NewIron)
    {
        NewIron->SetForm(EBlockForm::Metal);
        NewIron->SetActorScale3D(FVector(0.6f, 0.6f, 0.6f));
        if (NewIron->MeshComp)
        {
            NewIron->MeshComp->SetWorldScale3D(FVector(0.6f, 0.6f, 0.6f));
        }

        FIronSpawnData NewData;
        NewData.IronActor = NewIron;
        NewData.TimeInZone = 0.0f;
        NewData.bIsInZone = false; 

        SpawnedIronList.Add(NewData);
    }
}

void AIronSpawner::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
                                      bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor) return;

    // 감지 로그 출력 (삭제가 안 될 때 박스가 정상 작동하는지 확인용)
    UE_LOG(LogTemp, Warning, TEXT("삭제 구역에 무언가 들어옴: %s"), *OtherActor->GetName());

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

    // 탈출 로그 출력
    UE_LOG(LogTemp, Warning, TEXT("삭제 구역에서 벗어남: %s"), *OtherActor->GetName());

    for (int32 i = 0; i < SpawnedIronList.Num(); ++i)
    {
        if (SpawnedIronList[i].IronActor == OtherActor)
        {
            SpawnedIronList[i].bIsInZone = false;
            SpawnedIronList[i].TimeInZone = 0.0f; // 타이머 초기화 (살려줌)
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
                UE_LOG(LogTemp, Error, TEXT("방치 시간 초과로 철 파괴: %s"), *Iron->GetName());
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