#include "materialCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Components/InputComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"   

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputActionValue.h"

#include "UObject/ConstructorHelpers.h"
#include "Transformation_actor.h"

AmaterialCharacter::AmaterialCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	AutoPossessPlayer = EAutoReceiveInput::Player0;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 350.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 80.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 6.0f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 12.0f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom);
	FollowCamera->bUsePawnControlRotation = false;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(
		TEXT("SkeletalMesh'/Game/modeling/Character/Astronier.Astronier'")
	);
	if (MeshAsset.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MeshAsset.Object);
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
		GetMesh()->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
	}

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC_DefaultAsset(
		TEXT("InputMappingContext'/Game/Input/IMC_Default.IMC_Default'")
	);
	if (IMC_DefaultAsset.Succeeded()) IMC_Default = IMC_DefaultAsset.Object;

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC_MouseLookAsset(
		TEXT("InputMappingContext'/Game/Input/IMC_MouseLook.IMC_MouseLook'")
	);
	if (IMC_MouseLookAsset.Succeeded()) IMC_MouseLook = IMC_MouseLookAsset.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> IA_MoveAsset(
		TEXT("InputAction'/Game/Input/Actions/IA_Move.IA_Move'")
	);
	if (IA_MoveAsset.Succeeded()) IA_Move = IA_MoveAsset.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> IA_LookAsset(
		TEXT("InputAction'/Game/Input/Actions/IA_Look.IA_Look'")
	);
	if (IA_LookAsset.Succeeded()) IA_Look = IA_LookAsset.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> IA_MouseLookAsset(
		TEXT("InputAction'/Game/Input/Actions/IA_MouseLook.IA_MouseLook'")
	);
	if (IA_MouseLookAsset.Succeeded()) IA_MouseLook = IA_MouseLookAsset.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> IA_JumpAsset(
		TEXT("InputAction'/Game/Input/Actions/IA_Jump.IA_Jump'")
	);
	if (IA_JumpAsset.Succeeded()) IA_Jump = IA_JumpAsset.Object;

	if (PickupTags.Num() == 0)
	{
	PickupTags.Add(TEXT("Metal"));
	PickupTags.Add(TEXT("Rubber"));
	PickupTags.Add(TEXT("Ice"));
	PickupTags.Add(TEXT("Wood"));
	}

	BackpackComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackpackComp"));
	BackpackComp->SetupAttachment(GetMesh()); 

	BackpackComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackpackComp->SetGenerateOverlapEvents(false);
	BackpackComp->SetSimulatePhysics(false);
	BackpackComp->SetEnableGravity(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BackpackMeshAsset(
	TEXT("StaticMesh'/Game/modeling/Character/backPack/BackPack_final.BackPack_final'")
	);
	if (BackpackMeshAsset.Succeeded())
	{
	BackpackComp->SetStaticMesh(BackpackMeshAsset.Object);
	}

}

void AmaterialCharacter::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP) return;

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem) return;

	if (IMC_Default) Subsystem->AddMappingContext(IMC_Default, 0);
	if (IMC_MouseLook) Subsystem->AddMappingContext(IMC_MouseLook, 1);
	
	if (GetMesh())
	{
		GetMesh()->SetRenderCustomDepth(true);
		GetMesh()->SetCustomDepthStencilValue(CustomDepthStencilValue);
	}	

	if (BackpackComp && GetMesh())
	{
	BackpackComp->AttachToComponent(
		GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale, 
		BackpackSocketName
	);

	BackpackComp->SetRelativeLocation(BackpackRelativeLocation);
	BackpackComp->SetRelativeRotation(BackpackRelativeRotation);
	BackpackComp->SetRelativeScale3D(BackpackRelativeScale); 
	}

}

void AmaterialCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction("ChangeForm", IE_Pressed, this, &AmaterialCharacter::ChangeForm);
	PlayerInputComponent->BindAction("Hold", IE_Pressed, this, &AmaterialCharacter::HoldPressed);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC) return;

	if (IA_Move)      EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AmaterialCharacter::Move);
	if (IA_Look)      EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AmaterialCharacter::Look);
	if (IA_MouseLook) EIC->BindAction(IA_MouseLook, ETriggerEvent::Triggered, this, &AmaterialCharacter::Look);

	if (IA_Jump)
	{
		EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &AmaterialCharacter::JumpStarted);
		EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AmaterialCharacter::JumpStopped);
	}
}

void AmaterialCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller) return;

	const float Yaw = Controller->GetControlRotation().Yaw;
	const FRotator YawRot(0.f, Yaw, 0.f);

	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right   = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Axis.Y);
	AddMovementInput(Right, Axis.X);
}

void AmaterialCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AmaterialCharacter::JumpStarted()
{
	Jump();
}

void AmaterialCharacter::JumpStopped()
{
	StopJumping();
}

void AmaterialCharacter::ChangeForm()
{
	if (!FollowCamera) return;

	const FVector Start = FollowCamera->GetComponentLocation();
	const FVector End = Start + FollowCamera->GetForwardVector() * InteractRange;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	if (!bHit) return;

	ATransformation_actor* TransformActor = Cast<ATransformation_actor>(Hit.GetActor());
	if (TransformActor)
	{
		TransformActor->NextForm();
	}
}

void AmaterialCharacter::HoldPressed()
{
	if (HeldActor)
	{
		DropHeld();
		return;
	}
	TryPickup();

	
	
}

bool AmaterialCharacter::TryPickup()
{
	if (!FollowCamera) return false;

	const FVector Start = FollowCamera->GetComponentLocation();
	const FVector End = Start + FollowCamera->GetForwardVector() * PickupRange;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Camera, Params);
	if (!bHit) return false;

	AActor* Target = Hit.GetActor();
	if (!Target) return false;

	bool bAllowed = false;
	for (const FName& Tag : PickupTags)
	{
	if (Target->ActorHasTag(Tag))
	{
		bAllowed = true;
		break;
	}
	}
	
	if (!bAllowed) return false;


	TArray<UPrimitiveComponent*> PrimComps;
	Target->GetComponents<UPrimitiveComponent>(PrimComps);
	for (UPrimitiveComponent* PC : PrimComps)
	{
		if (!PC) continue;
		PC->SetSimulatePhysics(false);
		PC->SetEnableGravity(false);
		PC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	Target->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, HoldSocketName);
	HeldActor = Target;
	return true;
}

void AmaterialCharacter::DropHeld()
{
	if (!HeldActor) return;

	HeldActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	TArray<UPrimitiveComponent*> PrimComps;
	HeldActor->GetComponents<UPrimitiveComponent>(PrimComps);
	for (UPrimitiveComponent* PC : PrimComps)
	{
		if (!PC) continue;
		PC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PC->SetEnableGravity(true);
		PC->SetSimulatePhysics(true);
	}

	HeldActor = nullptr;
}
