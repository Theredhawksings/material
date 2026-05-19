// Fill out your copyright notice in the Description page of Project Settings.


#include "IronSpawner.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"

AIronSpawner::AIronSpawner()
{
    PrimaryActorTick.bCanEverTick = true;

    // 스폰 위치 컴포넌트
    SpawnLocationComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnLocationComp"));
    SetRootComponent(SpawnLocationComponent);

    // ★ 바닥 파괴 영역 박스 컴포넌트 생성 및 루트에 부착
    DestructionZoneComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("DestructionZoneComp"));
    DestructionZoneComponent->SetupAttachment(RootComponent);
    
    // 에디터에서 보기 편하게 기본 박스 크기 지정 (가로 세로 높이)
    DestructionZoneComponent->SetBoxExtent(FVector(200.0f, 200.0f, 50.0f));
    // 겹침(Overlap) 이벤트만 다루도록 설정
    DestructionZoneComponent->SetCollisionProfileName(TEXT("Trigger"));

    // 기획 스펙 세팅
    SpawnInterval = 4.0f; 
    IronLifeTime = 3.5f;  
    IronClass = ATransformation_actor::StaticClass();
}

void AIronSpawner::BeginPlay()
{
    Super::BeginPlay();
    
    // 델리게이트 연결: 박스 구역에 무언가 들어오고 나갈 때 매니저가 알아챔
    DestructionZoneComponent->OnComponentBeginOverlap.AddDynamic(this, &AIronSpawner::OnZoneBeginOverlap);
    DestructionZoneComponent->OnComponentEndOverlap.AddDynamic(this, &AIronSpawner::OnZoneEndOverlap);

    // 4초 주기 스폰 시작
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
        NewData.bIsInZone = false; // 태어날 때는 공중이므로 false

        SpawnedIronList.Add(NewData);
    }
}

void AIronSpawner::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
                                      bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor) return;

    // 들어온 액터가 우리가 관리하는 철 목록에 있는지 검사
    for (int32 i = 0; i < SpawnedIronList.Num(); ++i)
    {
        if (SpawnedIronList[i].IronActor == OtherActor)
        {
            // 바닥 구역 진입 확인 -> 타이머 가동 시작 시그널
            SpawnedIronList[i].bIsInZone = true;
            break;
        }
    }
}

void AIronSpawner::OnZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
                                    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!OtherActor) return;

    // 플레이어가 들고 가거나 튕겨서 구역을 벗어난 경우
    for (int32 i = 0; i < SpawnedIronList.Num(); ++i)
    {
        if (SpawnedIronList[i].IronActor == OtherActor)
        {
            // 구역을 나갔으므로 타이머를 끄고 초기화 (파괴 면역)
            SpawnedIronList[i].bIsInZone = false;
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

        // ★ 오직 "바닥 파괴 영역 안에 들어와 있는 철"만 시간이 흘러감
        if (SpawnedIronList[i].bIsInZone)
        {
            SpawnedIronList[i].TimeInZone += DeltaTime;

            // 영역 내에서 3.5초를 버티면 방치된 것으로 보고 청소
            if (SpawnedIronList[i].TimeInZone >= IronLifeTime)
            {
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
            SpawnIron(); // 코일건 소비 시 즉시 보충
            break;
        }
    }
}