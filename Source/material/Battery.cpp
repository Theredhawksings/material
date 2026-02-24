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
	InteractionBox->SetBoxExtent(FVector(250.0f));
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	ConnectionOutlet = CreateDefaultSubobject<UBoxComponent>(TEXT("ConnectionOutlet"));
	ConnectionOutlet->SetupAttachment(RootComponent);
	ConnectionOutlet->SetBoxExtent(FVector(150.0f));
	ConnectionOutlet->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ConnectionOutlet->SetCollisionResponseToAllChannels(ECR_Overlap);
}

void ABATTERY::BeginPlay()
{
	Super::BeginPlay();

	InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &ABATTERY::OnInteractionBoxBeginOverlap);
	InteractionBox->OnComponentEndOverlap.AddDynamic(this, &ABATTERY::OnInteractionBoxEndOverlap);
	ConnectionOutlet->OnComponentBeginOverlap.AddDynamic(this, &ABATTERY::OnConnectionOverlap);
	ConnectionOutlet->OnComponentEndOverlap.AddDynamic(this, &ABATTERY::OnConnectionEndOverlap);

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
	if (!CachedPlayerController)
	{
		return;
	}

	if (!BatteryInputComponent)
	{
		BatteryInputComponent = NewObject<UInputComponent>(this, UInputComponent::StaticClass(), TEXT("BatteryInput"));
		BatteryInputComponent->RegisterComponent();
		BatteryInputComponent->BindAction("Hold", IE_Pressed, this, &ABATTERY::OnHoldPressed);
		BatteryInputComponent->BindAction("Hold", IE_Released, this, &ABATTERY::OnHoldReleased);
		BatteryInputComponent->Priority = 10;
	}

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

void ABATTERY::OnInteractionBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
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
	if (bPlayerInRange)
	{
		TogglePower();
	}
}

void ABATTERY::OnHoldReleased()
{
}

void ABATTERY::TogglePower()
{
	bPowered = !bPowered;
	RefreshConnectedWires();

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 2.0f, bPowered ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("Battery: %s  %.1fV | Wires: %d"),
				bPowered ? TEXT("ON") : TEXT("OFF"), Voltage, ConnectedWires.Num()));
	}
#endif
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
		if (Wire)
		{
			Wire->SetPowered(bPowered);
			Wire->SetBatteryVoltage(bPowered ? Voltage : 0.f);
		}
	}
}

void ABATTERY::OnConnectionOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (AWire* Wire = Cast<AWire>(OtherActor))
	{
		ConnectedWires.AddUnique(Wire);
		Wire->SetPowered(bPowered);
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