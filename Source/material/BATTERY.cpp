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

    UE_LOG(LogTemp, Log, TEXT("Battery BeginPlay"));

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
    UE_LOG(LogTemp, Log, TEXT("Battery: SetupInputBinding called"));

    if (!CachedPlayerController)
    {
        UE_LOG(LogTemp, Error, TEXT("Battery: CachedPlayerController is null!"));
        return;
    }

    if (!InputComponent)
    {
        InputComponent = NewObject<UInputComponent>(this, UInputComponent::StaticClass(), TEXT("BatteryInput"));
        InputComponent->RegisterComponent();
        UE_LOG(LogTemp, Log, TEXT("Battery: InputComponent created"));
    }

    InputComponent->BindAction("Hold", IE_Pressed, this, &ABATTERY::OnHoldPressed);
    InputComponent->BindAction("Hold", IE_Released, this, &ABATTERY::OnHoldReleased);

    EnableInput(CachedPlayerController);
    UE_LOG(LogTemp, Log, TEXT("Battery: Input enabled"));
}

void ABATTERY::RemoveInputBinding()
{
    if (CachedPlayerController && InputComponent)
    {
        DisableInput(CachedPlayerController);
        UE_LOG(LogTemp, Log, TEXT("Battery: Input disabled"));
    }
}

void ABATTERY::OnInteractionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, 
                                                AActor* OtherActor, 
                                                UPrimitiveComponent* OtherComp, 
                                                int32 OtherBodyIndex, 
                                                bool bFromSweep, 
                                                const FHitResult& SweepResult)
{
    UE_LOG(LogTemp, Log, TEXT("Battery: OnInteractionBoxBeginOverlap with %s"), *OtherActor->GetName());

    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (Character && Character->IsPlayerControlled())
    {
        CachedPlayerController = Cast<APlayerController>(Character->GetController());
        if (CachedPlayerController)
        {
            bPlayerInRange = true;
            SetupInputBinding();
            UE_LOG(LogTemp, Log, TEXT("Battery: Player entered range"));
        }
    }
}

void ABATTERY::OnInteractionBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, 
                                              AActor* OtherActor, 
                                              UPrimitiveComponent* OtherComp, 
                                              int32 OtherBodyIndex)
{
    UE_LOG(LogTemp, Log, TEXT("Battery: OnInteractionBoxEndOverlap with %s"), *OtherActor->GetName());

    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (Character && Character->IsPlayerControlled())
    {
        bPlayerInRange = false;
        RemoveInputBinding();
        CachedPlayerController = nullptr;
        UE_LOG(LogTemp, Log, TEXT("Battery: Player left range"));
    }
}

void ABATTERY::OnHoldPressed()
{
    UE_LOG(LogTemp, Warning, TEXT("Battery: OnHoldPressed called! bPlayerInRange = %s"), bPlayerInRange ? TEXT("TRUE") : TEXT("FALSE"));

    if (bPlayerInRange)
    {
        TogglePower();
    }
}

void ABATTERY::OnHoldReleased()
{
    UE_LOG(LogTemp, Log, TEXT("Battery: OnHoldReleased called"));
}

void ABATTERY::TogglePower()
{
    bPowered = !bPowered;
    UE_LOG(LogTemp, Warning, TEXT("Battery: TogglePower - bPowered is now %s"), bPowered ? TEXT("TRUE") : TEXT("FALSE"));

    UpdateWiresPower();
}

void ABATTERY::RefreshConnectedWires()
{
    ConnectedWires.Empty();

    TArray<AActor*> OverlappingActors;
    ConnectionOutlet->GetOverlappingActors(OverlappingActors);

    UE_LOG(LogTemp, Warning, TEXT("=== BATTERY REFRESH START ==="));
    UE_LOG(LogTemp, Warning, TEXT("Found %d overlapping actors"), OverlappingActors.Num());

    for (AActor* Actor : OverlappingActors)
    {
        if (!Actor) continue;
        
        // 자기 자신 제외!
        if (Actor == this)
        {
            UE_LOG(LogTemp, Log, TEXT("  - Skipping self (Battery)"));
            continue;
        }

        UE_LOG(LogTemp, Warning, TEXT("  - Actor: %s (Class: %s)"), 
               *Actor->GetName(), *Actor->GetClass()->GetName());
        
        AWire* Wire = Cast<AWire>(Actor);
        if (Wire)
        {
            ConnectedWires.AddUnique(Wire);
            UE_LOG(LogTemp, Warning, TEXT("    ✓ Added Wire!"));
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("    ✗ Not a Wire"));
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Total wires: %d"), ConnectedWires.Num());
    UE_LOG(LogTemp, Warning, TEXT("=== BATTERY REFRESH END ==="));
    
    UpdateWiresPower();
}

void ABATTERY::UpdateWiresPower()
{
    UE_LOG(LogTemp, Warning, TEXT("Battery: UpdateWiresPower called - Updating %d wires to bPowered=%s"), 
           ConnectedWires.Num(), bPowered ? TEXT("TRUE") : TEXT("FALSE"));

    for (AActor* WireActor : ConnectedWires)
    {
        if (AWire* Wire = Cast<AWire>(WireActor))
        {
            UE_LOG(LogTemp, Warning, TEXT("Battery: Calling SetPowered(%s) on Wire: %s"), 
                   bPowered ? TEXT("TRUE") : TEXT("FALSE"), *Wire->GetName());
            Wire->SetPowered(bPowered);
        }
    }
}