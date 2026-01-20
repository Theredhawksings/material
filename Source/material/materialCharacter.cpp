#include "materialCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "InputAction.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

AmaterialCharacter::AmaterialCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 300.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom);
	FollowCamera->bUsePawnControlRotation = false;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(
		TEXT("/Game/modeling/Character/Astronier.Astronier")
	);

	if (MeshAsset.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MeshAsset.Object);
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
		GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	}

	// Input Actions 로드
	static ConstructorHelpers::FObjectFinder<UInputAction> MoveActionAsset(
		TEXT("/Game/Input/Actions/IA_Move")
	);
	if (MoveActionAsset.Succeeded())
	{
		MoveAction = MoveActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> LookActionAsset(
		TEXT("/Game/Input/Actions/IA_Look")
	);
	if (LookActionAsset.Succeeded())
	{
		LookAction = LookActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> MouseLookActionAsset(
		TEXT("/Game/Input/Actions/IA_MouseLook")
	);
	if (MouseLookActionAsset.Succeeded())
	{
		MouseLookAction = MouseLookActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> JumpActionAsset(
		TEXT("/Game/Input/Actions/IA_Jump")
	);
	if (JumpActionAsset.Succeeded())
	{
		JumpAction = JumpActionAsset.Object;
	}
}

void AmaterialCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("Character BeginPlay called"));
}

void AmaterialCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC) 
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("Failed to cast to EnhancedInputComponent"));
		return;
	}

	if (MoveAction)
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AmaterialCharacter::Move);
		UE_LOG(LogTemplateCharacter, Log, TEXT("MoveAction bound successfully"));
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("MoveAction is NULL"));
	}

	if (LookAction)
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AmaterialCharacter::Look);
		UE_LOG(LogTemplateCharacter, Log, TEXT("LookAction bound successfully"));
	}

	if (MouseLookAction)
	{
		EIC->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AmaterialCharacter::Look);
		UE_LOG(LogTemplateCharacter, Log, TEXT("MouseLookAction bound successfully"));
	}

	if (JumpAction)
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &AmaterialCharacter::DoJumpStart);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &AmaterialCharacter::DoJumpEnd);
		UE_LOG(LogTemplateCharacter, Log, TEXT("JumpAction bound successfully"));
	}
}

void AmaterialCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	UE_LOG(LogTemplateCharacter, Log, TEXT("Move called: X=%f, Y=%f"), Axis.X, Axis.Y);
	DoMove(Axis.X, Axis.Y);
}

void AmaterialCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	DoLook(Axis.X, Axis.Y);
}

void AmaterialCharacter::DoMove(float Right, float Forward)
{
	if (!Controller) 
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("Controller is NULL in DoMove"));
		return;
	}

	const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);

	const FVector ForwardDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector RightDir   = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDir, Forward);
	AddMovementInput(RightDir, Right);
}

void AmaterialCharacter::DoLook(float Yaw, float Pitch)
{
	AddControllerYawInput(Yaw);
	AddControllerPitchInput(Pitch);
}

void AmaterialCharacter::DoJumpStart()
{
	Jump();
}

void AmaterialCharacter::DoJumpEnd()
{
	StopJumping();
}