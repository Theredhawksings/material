#include "Battery.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Wire.h" // 프로젝트의 실제 전선 헤더 이름 확인 필요

ABATTERY::ABATTERY()
{
    PrimaryActorTick.bCanEverTick = false;

    BatteryMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BatteryMesh"));
    RootComponent = BatteryMesh;

    InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
    InteractionBox->SetupAttachment(RootComponent);
    InteractionBox->SetBoxExtent(FVector(250.0f, 250.0f, 250.0f));
    InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    ConnectionOutlet = CreateDefaultSubobject<UBoxComponent>(TEXT("ConnectionOutlet"));
    ConnectionOutlet->SetupAttachment(RootComponent);
    // [판정 상향] 전선 연결 박스 크기를 더 넉넉하게 설정
    ConnectionOutlet->SetBoxExtent(FVector(150.0f, 150.0f, 150.0f)); 
    ConnectionOutlet->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ConnectionOutlet->SetCollisionResponseToAllChannels(ECR_Overlap);

    bPowered = false;
    bPlayerInRange = false;
    CachedPlayerController = nullptr;
    BatteryInputComponent = nullptr;
}

void ABATTERY::BeginPlay()
{
    Super::BeginPlay();

    InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &ABATTERY::OnInteractionBoxBeginOverlap);
    InteractionBox->OnComponentEndOverlap.AddDynamic(this, &ABATTERY::OnInteractionBoxEndOverlap);

    // [추가] 실시간 오버랩 감지 (전선을 갖다 대는 즉시 반응)
    ConnectionOutlet->OnComponentBeginOverlap.AddDynamic(this, &ABATTERY::OnConnectionOverlap);
    ConnectionOutlet->OnComponentEndOverlap.AddDynamic(this, &ABATTERY::OnConnectionEndOverlap);

    RefreshConnectedWires();
    GetWorld()->GetTimerManager().SetTimer(RefreshTimerHandle, this, &ABATTERY::RefreshConnectedWires, 0.2f, true);
}

void ABATTERY::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemoveInputBinding();
    Super::EndPlay(EndPlayReason);
}

void ABATTERY::SetupInputBinding()
{
    if (!CachedPlayerController) return;

    // [수정] 컴포넌트가 없을 때만 생성하고 바인딩은 딱 한 번만 수행
    if (!BatteryInputComponent)
    {
        BatteryInputComponent = NewObject<UInputComponent>(this, UInputComponent::StaticClass(), TEXT("BatteryInputInstance"));
        BatteryInputComponent->RegisterComponent();
        BatteryInputComponent->BindAction("Hold", IE_Pressed, this, &ABATTERY::OnHoldPressed);
        BatteryInputComponent->BindAction("Hold", IE_Released, this, &ABATTERY::OnHoldReleased);
        // 필요 시 우선순위 상향
        BatteryInputComponent->Priority = 10;
    }

    // 현재 액터의 InputComponent에 우리가 만든 것을 할당하고 활성화
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
}

void ABATTERY::OnInteractionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (Character && Character->IsPlayerControlled())
    {
        CachedPlayerController = Cast<APlayerController>(Character->GetController());
        if (CachedPlayerController)
        {
            bPlayerInRange = true;
            SetupInputBinding();
        }
    }
}

void ABATTERY::OnInteractionBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (Character && Character->IsPlayerControlled())
    {
        bPlayerInRange = false;
        RemoveInputBinding();
        CachedPlayerController = nullptr;
    }
}

void ABATTERY::OnHoldPressed()
{
    if (bPlayerInRange)
    {
        TogglePower();
    }
}

void ABATTERY::OnHoldReleased() {}

void ABATTERY::TogglePower()
{
    bPowered = !bPowered;

    // 전기를 보내기 직전 즉시 갱신
    RefreshConnectedWires();
    UpdateWiresPower();

    if (GEngine) {
        GEngine->AddOnScreenDebugMessage(1, 2.0f, bPowered ? FColor::Green : FColor::Red, 
            FString::Printf(TEXT("Battery Toggle: %s | Wires: %d"), bPowered ? TEXT("ON") : TEXT("OFF"), ConnectedWires.Num()));
    }
}

void ABATTERY::RefreshConnectedWires()
{
    ConnectedWires.Empty();

    TArray<AActor*> OverlappingActors;
    ConnectionOutlet->GetOverlappingActors(OverlappingActors);

    for (AActor* Actor : OverlappingActors)
    {
        if (!Actor || Actor == this) continue;

        AWire* Wire = Cast<AWire>(Actor);
        if (Wire)
        {
            ConnectedWires.AddUnique(Wire);
        }
    }
    
    UpdateWiresPower();
}

void ABATTERY::UpdateWiresPower()
{
    for (AActor* WireActor : ConnectedWires)
    {
        if (AWire* Wire = Cast<AWire>(WireActor))
        {
            Wire->SetPowered(bPowered);
        }
    }
}

void ABATTERY::OnConnectionOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (AWire* Wire = Cast<AWire>(OtherActor))
    {
        ConnectedWires.AddUnique(Wire);
        Wire->SetPowered(bPowered);
    }
}

void ABATTERY::OnConnectionEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (AWire* Wire = Cast<AWire>(OtherActor))
    {
        ConnectedWires.Remove(Wire);
        Wire->SetPowered(false);
    }
}