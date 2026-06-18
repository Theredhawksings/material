#include "MainStage1_Platform2.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"   // ★ 추가
#include "Sound/SoundBase.h"              // ★ 추가
#include "Kismet/GameplayStatics.h"       // ★ 추가

AMainStage1_Platform2::AMainStage1_Platform2()
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

    // ★ 문 열리는 효과음 로드
    static ConstructorHelpers::FObjectFinder<USoundBase> DoorOpenAsset(
        TEXT("/Script/Engine.SoundWave'/Game/Sound/sound_door_opening.sound_door_opening'"));
    if (DoorOpenAsset.Succeeded())
        DoorOpenSound = DoorOpenAsset.Object;
}

void AMainStage1_Platform2::BeginPlay()
{
    Super::BeginPlay();

    // 문 시작/끝 위치 계산
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

    // Overlap 이벤트 바인딩
    TriggerBox->OnComponentBeginOverlap.AddDynamic(
        this, &AMainStage1_Platform2::OnOverlapBegin);
    TriggerBox->OnComponentEndOverlap.AddDynamic(
        this, &AMainStage1_Platform2::OnOverlapEnd);
}

void AMainStage1_Platform2::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 문 열기 애니메이션
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
                    FColor::Green, TEXT("✅ 문 열림 완료!"));
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
        const FColor BoxColor = bActivated ? FColor::Green : FColor::Red;
        DrawDebugBox(GetWorld(),
            TriggerBox->GetComponentLocation(),
            TriggerBox->GetScaledBoxExtent(),
            TriggerBox->GetComponentQuat(),
            BoxColor, false, -1.f, 0, 2.f);

        DrawDebugString(GetWorld(),
            GetActorLocation() + FVector(0, 0, 80.f),
            FString::Printf(TEXT("[MetalTarget]\n상태: %s\n태그: %s"),
                bActivated ? TEXT("활성화됨") : TEXT("대기중"),
                *RequiredTag.ToString()),
            nullptr,
            bActivated ? FColor::Green : FColor::Yellow,
            0.f, true);
    }
#endif
}

void AMainStage1_Platform2::OnOverlapBegin(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!OtherActor || bActivated) return;

    // Metal 태그 확인
    if (!OtherActor->ActorHasTag(RequiredTag)) return;

    // ★ 문 열기 시작
    bActivated = true;
    bIsOpening = true;
    CurrentTime = 0.0f;

    // ★ 문 열리는 효과음 (조건 충족 시 딱 한 번만 재생)
    if (DoorOpenSound)
        UGameplayStatics::PlaySoundAtLocation(this, DoorOpenSound, GetActorLocation());

    // 문 충돌 제거 (열리는 동안 막히지 않게)
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
            FColor::Green, TEXT("✅ 철 감지! 문이 열립니다"));

    UE_LOG(LogTemp, Log, TEXT("MetalTarget: %s 감지 → 문 열기"),
        *OtherActor->GetName());
}

void AMainStage1_Platform2::OnOverlapEnd(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32)
{
    // 한 번 열리면 닫히지 않음 (bActivated 유지)
}