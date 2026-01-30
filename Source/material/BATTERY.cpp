#include "Battery.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Wire.h"

ABATTERY::ABATTERY()
{
    PrimaryActorTick.bCanEverTick = false;

    BatteryMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BatteryMesh"));
    RootComponent = BatteryMesh;

    InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
    InteractionBox->SetupAttachment(RootComponent);
    InteractionBox->SetBoxExtent(FVector(200.0f, 200.0f, 200.0f));
    InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    ConnectionOutlet = CreateDefaultSubobject<UBoxComponent>(TEXT("ConnectionOutlet"));
    ConnectionOutlet->SetupAttachment(RootComponent);
    ConnectionOutlet->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
    ConnectionOutlet->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ConnectionOutlet->SetCollisionResponseToAllChannels(ECR_Overlap);

    bPowered = false;
    bPlayerInRange = false;
    CachedPlayerController = nullptr;
}

void ABATTERY::BeginPlay()
{
    Super::BeginPlay();

    InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &ABATTERY::OnInteractionBoxBeginOverlap);
    InteractionBox->OnComponentEndOverlap.AddDynamic(this, &ABATTERY::OnInteractionBoxEndOverlap);

    GetWorld()->GetTimerManager().SetTimer(RefreshTimerHandle, this, &ABATTERY::RefreshConnectedWires, 0.2f, false);
}

void ABATTERY::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemoveInputBinding();
    Super::EndPlay(EndPlayReason);
}

void ABATTERY::SetupInputBinding()
{
    if (!CachedPlayerController) return;

    if (!InputComponent)
    {
        InputComponent = NewObject<UInputComponent>(this, UInputComponent::StaticClass(), TEXT("BatteryInput"));
        InputComponent->RegisterComponent();
    }

    InputComponent->BindAction("Hold", IE_Pressed, this, &ABATTERY::OnHoldPressed);
    InputComponent->BindAction("Hold", IE_Released, this, &ABATTERY::OnHoldReleased);

    EnableInput(CachedPlayerController);
}

void ABATTERY::RemoveInputBinding()
{
    if (CachedPlayerController && InputComponent)
    {
        DisableInput(CachedPlayerController);
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

void ABATTERY::OnHoldReleased()
{
    // 필요한 경우 로직 추가
}

void ABATTERY::TogglePower()
{
    bPowered = !bPowered;
    UpdateWiresPower();
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