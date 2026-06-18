#include "Battery.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Wire.h"
#include "UObject/ConstructorHelpers.h"   // ★ 추가
#include "Sound/SoundBase.h"              // ★ 추가
#include "Kismet/GameplayStatics.h"       // ★ 추가
#include "Components/AudioComponent.h"   // ★ 추가

ABATTERY::ABATTERY()
{
    PrimaryActorTick.bCanEverTick = false;

    BatteryMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BatteryMesh"));
    RootComponent = BatteryMesh;

    InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
    InteractionBox->SetupAttachment(RootComponent);
    InteractionBox->SetBoxExtent(FVector(250.0f));
    InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    InteractionBox->bDrawOnlyIfSelected = !bShowDebugShapes;
    InteractionBox->ShapeColor = FColor::Green;
    InteractionBox->SetHiddenInGame(!bShowDebugShapes);
    InteractionBox->SetVisibility(bShowDebugShapes);

    ConnectionOutlet = CreateDefaultSubobject<UBoxComponent>(TEXT("ConnectionOutlet"));
    ConnectionOutlet->SetupAttachment(RootComponent);
    ConnectionOutlet->SetBoxExtent(FVector(150.0f));
    ConnectionOutlet->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ConnectionOutlet->SetCollisionResponseToAllChannels(ECR_Overlap);
    ConnectionOutlet->bDrawOnlyIfSelected = !bShowDebugShapes;
    ConnectionOutlet->ShapeColor = FColor::Yellow;
    ConnectionOutlet->SetHiddenInGame(!bShowDebugShapes);
    ConnectionOutlet->SetVisibility(bShowDebugShapes);

    static ConstructorHelpers::FObjectFinder<USoundBase> SparkAsset(
        TEXT("/Script/Engine.SoundWave'/Game/Sound/sound_Spark.sound_Spark'"));
    if (SparkAsset.Succeeded())
        SparkSound = SparkAsset.Object;
    // ★ 스파크 루프용 오디오 컴포넌트 (이게 빠져 있었음)
    SparkAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("SparkAudioComp"));
    SparkAudioComp->SetupAttachment(RootComponent);
    SparkAudioComp->bAutoActivate = false;   // 시작 시 자동 재생 X
    if (SparkSound)
        SparkAudioComp->SetSound(SparkSound);
        
}

void ABATTERY::BeginPlay()
{
    Super::BeginPlay();

    InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &ABATTERY::OnInteractionBoxBeginOverlap);
    InteractionBox->OnComponentEndOverlap.AddDynamic(this, &ABATTERY::OnInteractionBoxEndOverlap);
    ConnectionOutlet->OnComponentBeginOverlap.AddDynamic(this, &ABATTERY::OnConnectionOverlap);
    ConnectionOutlet->OnComponentEndOverlap.AddDynamic(this, &ABATTERY::OnConnectionEndOverlap);

    BatteryMesh->SetRenderCustomDepth(true);
    BatteryMesh->SetCustomDepthStencilValue(0);

    ApplyDebugVisibility();

    RefreshConnectedWires();
    GetWorld()->GetTimerManager().SetTimer(RefreshTimerHandle, this,
        &ABATTERY::RefreshConnectedWires, 0.2f, true);
}

void ABATTERY::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorld()->GetTimerManager().ClearTimer(RefreshTimerHandle);
    RemoveInputBinding();
    Super::EndPlay(EndPlayReason);
}

void ABATTERY::SetupInputBinding()
{
    if (!CachedPlayerController) return;

    if (BatteryInputComponent) return;

    BatteryInputComponent = NewObject<UInputComponent>(this, UInputComponent::StaticClass(), TEXT("BatteryInput"));
    BatteryInputComponent->RegisterComponent();
    BatteryInputComponent->BindAction("Interaction", IE_Pressed, this, &ABATTERY::OnHoldPressed);
    BatteryInputComponent->BindAction("Interaction", IE_Released, this, &ABATTERY::OnHoldReleased);
    BatteryInputComponent->Priority = 10;

    InputComponent = BatteryInputComponent;
    EnableInput(CachedPlayerController);
}

void ABATTERY::RemoveInputBinding()
{
    if (CachedPlayerController)
    {
        DisableInput(CachedPlayerController);
        InputComponent = nullptr;
    }

    if (BatteryInputComponent)
    {
        BatteryInputComponent->UnregisterComponent();
        BatteryInputComponent->DestroyComponent();
        BatteryInputComponent = nullptr;
    }
}

void ABATTERY::OnInteractionBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (bPlayerInRange) return; // ★ 이미 범위 안이면 스킵

    if (const ACharacter* Character = Cast<ACharacter>(OtherActor))
    {
        if (Character->IsPlayerControlled())
        {
            CachedPlayerController = Cast<APlayerController>(Character->GetController());
            bPlayerInRange = true;
            SetupInputBinding();
        }
    }
}

void ABATTERY::OnInteractionBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (const ACharacter* Character = Cast<ACharacter>(OtherActor))
    {
        if (Character->IsPlayerControlled())
        {
            bPlayerInRange = false;
            RemoveInputBinding();
            CachedPlayerController = nullptr;
        }
    }
}

void ABATTERY::OnHoldPressed()
{
    if (bPlayerInRange) TogglePower();
}

void ABATTERY::OnHoldReleased()
{
}

void ABATTERY::TogglePower()
{
    bPowered = !bPowered;

    // ★ ON이면 계속 재생, OFF면 재생 중인 것 정지
    if (SparkAudioComp)
    {
        if (bPowered)
            SparkAudioComp->Play();
        else
            SparkAudioComp->Stop();
    }

    UE_LOG(LogTemp, Warning,
        TEXT("Battery Power: %s"),
        bPowered ? TEXT("ON") : TEXT("OFF"));

    BatteryMesh->SetCustomDepthStencilValue(
        bPowered ? 120 : 0);

    RefreshConnectedWires();
}

void ABATTERY::RefreshConnectedWires()
{
    ConnectedWires.Empty();

    TArray<AActor*> OverlappingActors;
    ConnectionOutlet->GetOverlappingActors(OverlappingActors);

    for (AActor* Actor : OverlappingActors)
    {
        if (AWire* Wire = Cast<AWire>(Actor))
        {
            ConnectedWires.AddUnique(Wire);
        }
    }

    UpdateWiresPower();
}

void ABATTERY::UpdateWiresPower()
{
    for (AWire* Wire : ConnectedWires)
    {
        if (!Wire) continue;

        Wire->SetBatterySource(bPowered);

        if (!bPowered)
        {
            TSet<AWire*> Visited;
            Wire->ResetVoltageNetwork(Visited);
            Wire->SetPowered(false);
        }
        else
        {
            Wire->SetBatteryVoltage(Voltage);
            Wire->SetPowered(true);
        }
    }
}

void ABATTERY::ApplyDebugVisibility()
{
    // InteractionBox 가시성
    if (InteractionBox)
    {
        InteractionBox->SetHiddenInGame(!bShowDebugShapes);
        InteractionBox->SetVisibility(bShowDebugShapes);
        InteractionBox->bDrawOnlyIfSelected = !bShowDebugShapes;
        InteractionBox->MarkRenderStateDirty();
    }

    // ConnectionOutlet 가시성
    if (ConnectionOutlet)
    {
        ConnectionOutlet->SetHiddenInGame(!bShowDebugShapes);
        ConnectionOutlet->SetVisibility(bShowDebugShapes);
        ConnectionOutlet->bDrawOnlyIfSelected = !bShowDebugShapes;
        ConnectionOutlet->MarkRenderStateDirty();
    }
}

#if WITH_EDITOR
void ABATTERY::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropName = (PropertyChangedEvent.Property != nullptr)
        ? PropertyChangedEvent.Property->GetFName()
        : NAME_None;

    if (PropName == GET_MEMBER_NAME_CHECKED(ABATTERY, bShowDebugShapes))
    {
        ApplyDebugVisibility();
    }
}
#endif

void ABATTERY::OnConnectionOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (AWire* Wire = Cast<AWire>(OtherActor))
    {
        ConnectedWires.AddUnique(Wire);

        const FVector OutletPos  = ConnectionOutlet->GetComponentLocation();
        const float DistToStart  = FVector::Dist(OutletPos, Wire->GetStartPointLocation());
        const float DistToEnd    = FVector::Dist(OutletPos, Wire->GetEndPointLocation());
        const bool bStartIsInput = (DistToStart <= DistToEnd);

        Wire->SetPowered(bPowered, bStartIsInput);
        Wire->SetBatteryVoltage(bPowered ? Voltage : 0.f);
    }
}

void ABATTERY::OnConnectionEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (AWire* Wire = Cast<AWire>(OtherActor))
    {
        ConnectedWires.Remove(Wire);
        Wire->SetPowered(false);
        Wire->SetBatteryVoltage(0.f);
    }
}