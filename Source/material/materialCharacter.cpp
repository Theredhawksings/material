#include "materialCharacter.h"
#include "materialAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Animation/AnimSequence.h"
#include "UObject/ConstructorHelpers.h"
#include "Transformation_actor.h"
#include "Syringe.h"
#include "TimerManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Blueprint/UserWidget.h"

FVector AmaterialCharacter::PendingSpawnLocation = FVector::ZeroVector;
bool    AmaterialCharacter::bHasPendingSpawn    = false;

// 틱 경로에서 FName을 매번 새로 만들지 않도록 태그 상수로 재사용
static const FName Tag_Metal(TEXT("Metal"));
static const FName Tag_Ice(TEXT("Ice"));
static const FName Tag_Copper(TEXT("Copper"));
static const FName Tag_Magnet(TEXT("Magnet"));
static const FName Tag_Rubber(TEXT("Rubber"));
static const FName Tag_Wood(TEXT("Wood"));
static const FName Tag_Joker(TEXT("Joker"));

AmaterialCharacter::AmaterialCharacter()
	: HeldActor(nullptr), PendingPickupActor(nullptr), bIsPlayingWalk(false), bWasHolding(false), bIsPickingUp(false)
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Player0;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent *Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, RotationRate, 0.f);
	Movement->JumpZVelocity = JumpVelocity;
	Movement->AirControl = AirControl;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = CameraArmLength;
	CameraBoom->SocketOffset = FVector(0.f, CameraSocketOffsetY, CameraSocketOffsetZ);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->CameraLagSpeed = CameraLagSpeed;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = CameraRotLagSpeed;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom);
	FollowCamera->bUsePawnControlRotation = false;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(
		TEXT("SkeletalMesh'/Game/modeling/Character/Astronier.Astronier'"));
	if (MeshAsset.Succeeded())
	{
		USkeletalMeshComponent *MeshComp = GetMesh();
		MeshComp->SetSkeletalMesh(MeshAsset.Object);
		MeshComp->SetRelativeLocation(FVector(0.f, 0.f, MeshOffsetZ));
		MeshComp->SetRelativeRotation(FRotator(0.f, MeshRotationYaw, 0.f));
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimBPAsset(
		TEXT("AnimBlueprint'/Game/modeling/Character/Astronier_Skeleton_AnimBlueprint.Astronier_Skeleton_AnimBlueprint_C'"));
	if (AnimBPAsset.Succeeded())
	{
		AnimBPClass = AnimBPAsset.Class;
		GetMesh()->SetAnimInstanceClass(AnimBPClass);
	}

	static ConstructorHelpers::FClassFinder<UTestUI> RadialMenuBP(
		TEXT("/Game/modeling/UI/Matarial_Change/WBP_RadialMenu"));
	if (RadialMenuBP.Succeeded())
	{
		RadialMenuClass = RadialMenuBP.Class;
	}

	static ConstructorHelpers::FClassFinder<UUserWidget> CrosshairBPClass(TEXT("/Game/Widget/WBP_Crosshair.WBP_Crosshair_C")); 
    if (CrosshairBPClass.Succeeded())
    {
        CrosshairWidgetClass = CrosshairBPClass.Class;
    }

	static ConstructorHelpers::FClassFinder<UUserWidget> PauseMenuBPClass(
    TEXT("/Game/Widget/PauseMenuWidget.PauseMenuWidget_C"));
	if (PauseMenuBPClass.Succeeded())
	{
    PauseMenuWidgetClass = PauseMenuBPClass.Class;
	}

	HoldPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HoldPivot"));
	HoldPivot->SetupAttachment(GetMesh());

	BackpackComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackpackComp"));
	BackpackComp->SetupAttachment(GetMesh());
	BackpackComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BackpackMeshAsset(
		TEXT("StaticMesh'/Game/modeling/Character/backPack/BackPack_final.BackPack_final'"));
	if (BackpackMeshAsset.Succeeded())
		BackpackComp->SetStaticMesh(BackpackMeshAsset.Object);

	static ConstructorHelpers::FObjectFinder<UMaterial> BackpackMat(
		TEXT("Material'/Game/modeling/Character/backPack/M_BackPack.M_BackPack'"));
	if (BackpackMat.Succeeded())
		BackpackComp->SetMaterial(0, BackpackMat.Object);

	BackpackUIComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("BackpackUIComp"));
	BackpackUIComp->SetupAttachment(BackpackComp);
	BackpackUIComp->SetWidgetSpace(EWidgetSpace::World);
	BackpackUIComp->SetTwoSided(true);
	BackpackUIComp->SetDrawAtDesiredSize(false);
	BackpackUIComp->SetDrawSize(FVector2D(66.f, 154.f));
	BackpackUIComp->SetRedrawTime(0.1f);

	static ConstructorHelpers::FClassFinder<UUserWidget> BackpackUIBP(
		TEXT("/Game/modeling/Character/backPack/WBP_BackPack_UI"));
	if (BackpackUIBP.Succeeded())
	{
		BackpackUIComp->SetWidgetClass(BackpackUIBP.Class);
	}

	ArmComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArmComp"));
	ArmComp->SetupAttachment(GetMesh());
	ArmComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ArmMeshAsset(
		TEXT("/Script/Engine.StaticMesh'/Game/modeling/Character/Right_Arm/Right_Arm.Right_Arm'"));
	if (ArmMeshAsset.Succeeded())
	{
		ArmComp->SetStaticMesh(ArmMeshAsset.Object);
	}

	ArmComp2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArmComp2"));
	ArmComp2->SetupAttachment(GetMesh());
	ArmComp2->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ArmMeshAsset2(
		TEXT("/Script/Engine.StaticMesh'/Game/modeling/Character/Left_Arm/Arm.Arm'"));
	if (ArmMeshAsset2.Succeeded())
	{
		ArmComp2->SetStaticMesh(ArmMeshAsset2.Object);
	}

	struct FAnimLoader
	{
		const TCHAR *Path;
		TObjectPtr<UAnimSequence> *Target;
	};
	const FAnimLoader AnimAssets[] = {
		{TEXT("AnimSequence'/Game/modeling/Animation/Walk1.Walk1'"), &WalkAnim},
		{TEXT("AnimSequence'/Game/modeling/Animation/Test.Test'"), &IdleAnim},
		{TEXT("AnimSequence'/Game/modeling/Animation/bring.bring'"), &PickupAnim},
		{TEXT("AnimSequence'/Game/modeling/Animation/idle_bring2.idle_bring2'"), &IdleBringAnim},
		{TEXT("AnimSequence'/Game/modeling/Animation/Walk_bring1.Walk_bring1'"), &WalkBringAnim},
		{TEXT("AnimSequence'/Game/modeling/Animation/Use_E.Use_E'"), &UseEAnim},
		{TEXT("AnimSequence'/Game/modeling/Animation/Use_Left.Use_Left'"), &UseLeftAnim},
		{TEXT("AnimSequence'/Game/modeling/Animation/insert.insert'"), &InsertAnim}};

	for (const FAnimLoader &Loader : AnimAssets)
	{
		ConstructorHelpers::FObjectFinder<UAnimSequence> AnimAsset(Loader.Path);
		if (AnimAsset.Succeeded())
			*Loader.Target = AnimAsset.Object;
	}
	struct FSoundLoader
	{
		const TCHAR *Path;
		TObjectPtr<USoundBase> *Target;
	};
	const FSoundLoader SoundAssets[] = {
		{TEXT("SoundWave'/Game/Sound/sound_jumping.sound_jumping'"),   &JumpSound},
		{TEXT("SoundWave'/Game/Sound/sound_shooting.sound_shooting'"), &ShootSound},
		{TEXT("SoundWave'/Game/Sound/sound_touch_pad.sound_touch_pad'"), &TouchPadSound},
		{TEXT("SoundWave'/Game/Sound/sound_walking.sound_walking'"),   &WalkSound},
	};
	
	for (const FSoundLoader &Loader : SoundAssets)
	{
		ConstructorHelpers::FObjectFinder<USoundBase> SoundAsset(Loader.Path);
		if (SoundAsset.Succeeded())
			*Loader.Target = SoundAsset.Object;
	}

	// ★ 메탈릭 사운드 3종 로드 (무작위 재생용)
	const TCHAR* MetallicPaths[] = {
		TEXT("SoundWave'/Game/Sound/sound__metallic_1.sound__metallic_1'"),
		TEXT("SoundWave'/Game/Sound/sound_metallic_2.sound_metallic_2'"),
		TEXT("SoundWave'/Game/Sound/sound_metallic_3.sound_metallic_3'"),
	};
	for (const TCHAR* Path : MetallicPaths)
	{
		ConstructorHelpers::FObjectFinder<USoundBase> S(Path);
		if (S.Succeeded())
			MetallicSounds.Add(S.Object);
	}

	// ★ 고무 사운드 2종 로드 (무작위 재생용)
	const TCHAR* RubberPaths[] = {
		TEXT("SoundWave'/Game/Sound/sound_rubber_1.sound_rubber_1'"),
		TEXT("SoundWave'/Game/Sound/sound_rubber_2.sound_rubber_2'"),
	};
	for (const TCHAR* Path : RubberPaths)
	{
		ConstructorHelpers::FObjectFinder<USoundBase> S(Path);
		if (S.Succeeded())
			RubberSounds.Add(S.Object);
	}

	// ★ 나무 사운드 2종 로드 (무작위 재생용)
	const TCHAR* WoodPaths[] = {
		TEXT("SoundWave'/Game/Sound/sound_wood_1.sound_wood_1'"),
		TEXT("SoundWave'/Game/Sound/sound_wood_2.sound_wood_2'"),
	};
	for (const TCHAR* Path : WoodPaths)
	{
		ConstructorHelpers::FObjectFinder<USoundBase> S(Path);
		if (S.Succeeded())
			WoodSounds.Add(S.Object);
	}

	// 걷기 루프용 오디오 컴포넌트 (재생/정지 제어 위해 컴포넌트로)
	WalkAudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("WalkAudioComp"));
	WalkAudioComp->SetupAttachment(RootComponent);
	WalkAudioComp->bAutoActivate = false; // 시작 시 자동 재생 X

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC_DefaultAsset(
		TEXT("InputMappingContext'/Game/Input/IMC_Default.IMC_Default'"));
	if (IMC_DefaultAsset.Succeeded())
		IMC_Default = IMC_DefaultAsset.Object;

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC_MouseLookAsset(
		TEXT("InputMappingContext'/Game/Input/IMC_MouseLook.IMC_MouseLook'"));
	if (IMC_MouseLookAsset.Succeeded())
		IMC_MouseLook = IMC_MouseLookAsset.Object;

	struct FActionLoader
	{
		const TCHAR *Path;
		TObjectPtr<UInputAction> *Target;
	};
	const FActionLoader InputActions[] = {
		{TEXT("InputAction'/Game/Input/Actions/IA_Move.IA_Move'"), &IA_Move},
		{TEXT("InputAction'/Game/Input/Actions/IA_Look.IA_Look'"), &IA_Look},
		{TEXT("InputAction'/Game/Input/Actions/IA_MouseLook.IA_MouseLook'"), &IA_MouseLook},
		{TEXT("InputAction'/Game/Input/Actions/IA_Jump.IA_Jump'"), &IA_Jump},
		{TEXT("InputAction'/Game/Input/Actions/IA_LeftClick.IA_LeftClick'"), &IA_LeftClick},
		{TEXT("InputAction'/Game/Input/Actions/IA_Escape.IA_Escape'"), &IA_Escape},
	};
	for (const FActionLoader &Loader : InputActions)
	{
		ConstructorHelpers::FObjectFinder<UInputAction> ActionAsset(Loader.Path);
		if (ActionAsset.Succeeded())
			*Loader.Target = ActionAsset.Object;
	}

	if (PickupTags.Num() == 0)
		PickupTags = {TEXT("Metal"), TEXT("Copper"), TEXT("Rubber"), TEXT("Ice"), TEXT("Wood"), TEXT("Magnet")};

	static ConstructorHelpers::FObjectFinder<UMaterial> PlayerMat(
		TEXT("/Script/Engine.Material'/Game/modeling/Character/M_Character.M_Character'"));
	if (PlayerMat.Succeeded())
		GetMesh()->SetMaterial(0, PlayerMat.Object);

	bIsUsingSyringe = false;
}

USoundBase* AmaterialCharacter::GetRandomMetallicSound() const
{
	if (MetallicSounds.Num() == 0)
		return nullptr;
	const int32 Idx = FMath::RandRange(0, MetallicSounds.Num() - 1);
	return MetallicSounds[Idx];
}

USoundBase* AmaterialCharacter::GetRandomRubberSound() const
{
	if (RubberSounds.Num() == 0)
		return nullptr;
	const int32 Idx = FMath::RandRange(0, RubberSounds.Num() - 1);
	return RubberSounds[Idx];
}

USoundBase* AmaterialCharacter::GetRandomWoodSound() const
{
	if (WoodSounds.Num() == 0)
		return nullptr;
	const int32 Idx = FMath::RandRange(0, WoodSounds.Num() - 1);
	return WoodSounds[Idx];
}

void AmaterialCharacter::ScheduleDropSound(USoundBase* Sound)
{
	if (!Sound)
		return;

	const FVector SoundLoc = GetActorLocation();
	FTimerHandle DropSoundHandle;
	GetWorld()->GetTimerManager().SetTimer(DropSoundHandle,
		[this, Sound, SoundLoc]()
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, SoundLoc);
		},
		1.0f, false);
}

void AmaterialCharacter::TryPlayImpactSound(AActor* OtherActor, const FVector& Location)
{
	if (!OtherActor)
		return;

	USoundBase* Snd = nullptr;
	if (OtherActor->ActorHasTag(Tag_Metal)  ||
		OtherActor->ActorHasTag(Tag_Ice)    ||
		OtherActor->ActorHasTag(Tag_Copper) ||
		OtherActor->ActorHasTag(Tag_Magnet))
	{
		Snd = GetRandomMetallicSound();
	}
	else if (OtherActor->ActorHasTag(Tag_Wood))
	{
		Snd = GetRandomWoodSound();
	}

	if (!Snd)
		return;

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastImpactSoundTime < ImpactSoundCooldown)
		return;

	UGameplayStatics::PlaySoundAtLocation(this, Snd, Location);
	LastImpactSoundTime = Now;
}

void AmaterialCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (const APlayerController *PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem *Subsystem =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (IMC_Default)
				Subsystem->AddMappingContext(IMC_Default, 0);
			if (IMC_MouseLook)
				Subsystem->AddMappingContext(IMC_MouseLook, 1);
		}
	}

	USkeletalMeshComponent *MeshComp = GetMesh();
	if (!MeshComp)
		return;

	MeshComp->SetRenderCustomDepth(false);
	MeshComp->SetCustomDepthStencilValue(0);
	// 크로스페이드 지원 커스텀 AnimInstance 사용 (기존 SingleNode 즉시 전환 → 부드러운 전환)
	MeshComp->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	MeshComp->SetAnimInstanceClass(UmaterialAnimInstance::StaticClass());
	PlayAnimIfValid(IdleAnim, true);

	if (HoldPivot && MeshComp->DoesSocketExist(HoldSocketName))
	{
		HoldPivot->AttachToComponent(MeshComp,
									 FAttachmentTransformRules::SnapToTargetNotIncludingScale, HoldSocketName);
		HoldPivot->SetRelativeLocation(FVector::ZeroVector);
		HoldPivot->SetRelativeRotation(FRotator::ZeroRotator);
	}

	if (BackpackComp)
	{
		BackpackComp->AttachToComponent(MeshComp,
										FAttachmentTransformRules::SnapToTargetNotIncludingScale, BackpackSocketName);
		BackpackComp->SetRelativeLocation(BackpackRelativeLocation);
		BackpackComp->SetRelativeRotation(BackpackRelativeRotation);
		BackpackComp->SetRelativeScale3D(BackpackRelativeScale);
	}

	if (BackpackUIComp)
	{
		BackpackUIComp->SetBackgroundColor(FLinearColor::Transparent);
		BackpackUIComp->SetBlendMode(EWidgetBlendMode::Transparent);
		BackpackUIComp->SetRelativeLocation(FVector(0.f, -0.47f, -0.12f));
		BackpackUIComp->SetRelativeRotation(FRotator(0.f, 270.f, 0.f));
		BackpackUIComp->SetRelativeScale3D(FVector(0.018f, 0.010f, 0.008f));
	}

	if (ArmComp)
	{
		ArmComp->AttachToComponent(MeshComp,
								   FAttachmentTransformRules::SnapToTargetNotIncludingScale,
								   TEXT("hand_RSocket_0"));
		ArmComp->SetRelativeLocation(FVector(0.039308f, 0.063895f, -0.000174f));
		ArmComp->SetRelativeRotation(FRotator::MakeFromEuler(FVector(-170.00052f, 0.000013f, -89.999984f)));
		ArmComp->SetRelativeScale3D(FVector(0.1f, 0.14f, 0.14f));
	}

	if (ArmComp2)
	{
		ArmComp2->AttachToComponent(
			GetMesh(),
			FAttachmentTransformRules::KeepRelativeTransform,
			ArmSocketName2);
		ArmComp2->SetRelativeLocation(FVector(-0.031129f, 0.025846f, 0.032451f));
		ArmComp2->SetRelativeRotation(FRotator(0.701087f, 90.0f, 103.319739f));
		ArmComp2->SetRelativeScale3D(FVector(0.01f, 0.01f, 0.01f));
	}

	FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(
		this, &AmaterialCharacter::OnWindowFocusChanged);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
    DefaultGroundFriction      = Move->GroundFriction;
    DefaultBrakingDeceleration = Move->BrakingDecelerationWalking;
	}
	bWasOnIce = false;

	// ★ 고무 옆면 충돌 감지
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetNotifyRigidBodyCollision(true);
		Capsule->OnComponentHit.AddDynamic(this, &AmaterialCharacter::OnCapsuleHit);
	}

	if (bHasPendingSpawn)
	{
		SetActorLocation(PendingSpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
		bHasPendingSpawn = false;
	}

	if (CrosshairWidgetClass != nullptr)
    {
        CrosshairWidget = CreateWidget<UUserWidget>(GetWorld(), CrosshairWidgetClass);
        if (CrosshairWidget != nullptr)
        {
            CrosshairWidget->AddToViewport(); 
        }
    }

}

void AmaterialCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HeldActor)
	{
		UpdateHoldPivotTransform();

		if (BackpackComp)
		{
			BackpackComp->SetRelativeLocation(BackpackRelativeLocation);
			BackpackComp->SetRelativeRotation(BackpackRotWhenHolding);
		}

		UpdateHeldMagnetism();
	}

	UpdateAnimation();
	UpdateGroundFriction();

		// 걷기 발소리: 땅에서 움직일 때만 재생 (점프/픽업 중엔 정지)
	if (WalkAudioComp && WalkSound)
	{
		const bool bShouldPlayWalk = IsMoving()
			&& !GetCharacterMovement()->IsFalling()
			&& !bIsPickingUp && !bIsUsingSyringe;

		if (bShouldPlayWalk && !WalkAudioComp->IsPlaying())
		{
			WalkAudioComp->SetSound(WalkSound);
			WalkAudioComp->Play();
		}
		else if (!bShouldPlayWalk && WalkAudioComp->IsPlaying())
		{
			WalkAudioComp->Stop();
		}
	}
	
}

void AmaterialCharacter::SetupPlayerInputComponent(UInputComponent *PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction("ChangeForm", IE_Pressed, this, &AmaterialCharacter::ChangeForm);
	PlayerInputComponent->BindAction("Hold", IE_Pressed, this, &AmaterialCharacter::HoldPressed);
	PlayerInputComponent->BindAction("Checkweight", IE_Pressed, this, &AmaterialCharacter::CheckWeight);
	PlayerInputComponent->BindAction("laboratory", IE_Pressed, this, &AmaterialCharacter::OnWarpLaboratory);
	PlayerInputComponent->BindAction("Stage1", IE_Pressed, this, &AmaterialCharacter::OnWarpStage1);
	PlayerInputComponent->BindAction("Stage2", IE_Pressed, this, &AmaterialCharacter::OnWarpStage2);
	PlayerInputComponent->BindAction("Stage3", IE_Pressed, this, &AmaterialCharacter::OnResetMap);

	UEnhancedInputComponent *EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
		return;

	if (IA_Move)
		EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AmaterialCharacter::Move);
	if (IA_Look)
		EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AmaterialCharacter::Look);
	if (IA_MouseLook)
		EIC->BindAction(IA_MouseLook, ETriggerEvent::Triggered, this, &AmaterialCharacter::Look);

	if (IA_Jump)
	{
		EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &AmaterialCharacter::JumpStarted);
		EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AmaterialCharacter::JumpStopped);
	}

	if (IA_LeftClick)
		EIC->BindAction(IA_LeftClick, ETriggerEvent::Started, this, &AmaterialCharacter::OnLeftClick);
	if (IA_Escape)
		EIC->BindAction(IA_Escape, ETriggerEvent::Started, this, &AmaterialCharacter::OnEscapePressed);
}

void AmaterialCharacter::Move(const FInputActionValue &Value)
{
	if (!Controller || bIsPickingUp || bIsUsingSyringe)
		return;
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Axis.IsNearlyZero())
		return;
	const FRotationMatrix RotMatrix(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f));
	AddMovementInput(RotMatrix.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(RotMatrix.GetUnitAxis(EAxis::Y), Axis.X);
}

void AmaterialCharacter::Look(const FInputActionValue &Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AmaterialCharacter::JumpStarted()
{
	Jump();
	if (JumpSound)
		UGameplayStatics::PlaySound2D(this, JumpSound);
}

void AmaterialCharacter::JumpStopped() { StopJumping(); }

void AmaterialCharacter::ChangeForm()
{
	if (bIsPickingUp || HeldActor)
		return;

	if (bRadialMenuOpen)
	{
		CloseRadialMenu(false);
		return;
	}

	if (UseEAnim && GetMesh())
	{
		PlaySmoothAnim(UseEAnim, false);
		bIsPickingUp = true;

		// 애니 재생 후 0.2초 뒤에 터치패드 사운드
		FTimerHandle TouchSoundHandle;
		GetWorld()->GetTimerManager().SetTimer(TouchSoundHandle, [this]()
		{
			if (TouchPadSound)
				UGameplayStatics::PlaySound2D(this, TouchPadSound);
		}, 0.2f, false);

		GetWorld()->GetTimerManager().SetTimer(RadialMenuAnimTimer, this,
			&AmaterialCharacter::OnUseEAnimFinished, UseEAnim->GetPlayLength(), false);
	}
	else
	{
		OpenRadialMenu(nullptr);
	}
}

void AmaterialCharacter::CheckWeight()
{
	RubberGauge = 10;
	MetalGauge = 10;
	IceGauge = 10;
	WoodGauge = 10;
	MagnetGauge = 10;
	CopperGauge = 10;
}

void AmaterialCharacter::HoldPressed()
{
	if (bIsPickingUp || bRadialMenuOpen)
		return;

	if (AttachedSyringe && !bIsUsingSyringe)
	{
		UseSyringePressed();
		return;
	}

	if (HeldActor)
		DropHeld();
	else
		TryPickup();
}

void AmaterialCharacter::OnLeftClick()
{
	UE_LOG(LogTemp, Warning, TEXT("[Click] 진입 menuOpen=%d paused=%d captured=%d loaded=%d"),
		bRadialMenuOpen, bGamePaused, bMouseCaptured, bHasLoadedForm);

	if (bRadialMenuOpen) { CloseRadialMenu(true); return; }

	APlayerController *PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	if (bGamePaused) { PC->SetPause(false); bGamePaused = false; }

	if (!bMouseCaptured)
	{
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		bMouseCaptured = true;
		UE_LOG(LogTemp, Warning, TEXT("[Click] 마우스 재캡처만 함 - 이번 클릭은 발사 안 함"));
		return;
	}

	if (bHasLoadedForm)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Click] 발사!"));
		FireMaterialShot();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Click] 장전된 것 없음"));
	}
}

bool AmaterialCharacter::TryPickup()
{
	if (!FollowCamera || bIsPickingUp)
		return false;

	const FVector Start = FollowCamera->GetComponentLocation();
	const FVector End = Start + FollowCamera->GetForwardVector() * PickupRange + FollowCamera->GetRightVector() * -20.f;

	DrawDebugSphere(GetWorld(), Start, 8.f, 8, FColor::White, false, 3.0f, 0, 1.f);
	DrawDebugSphere(GetWorld(), End, PickupSphereRadius, 12, FColor::Blue, false, 3.0f);
	DrawDebugLine(GetWorld(), Start, End, FColor::Blue, false, 3.0f, 0, 1.5f);
	DrawDebugSphere(GetWorld(), GetActorLocation(), 15.f, 8, FColor(128, 128, 128), false, 3.0f);

	UE_LOG(LogTemp, Warning, TEXT("[TryPickup] CameraPos=(%s) | Forward=(%s) | Range=%.0f | SphereR=%.0f"),
		   *Start.ToCompactString(),
		   *FollowCamera->GetForwardVector().ToCompactString(),
		   PickupRange,
		   PickupSphereRadius);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TryPickup), false, this);
	const bool bHit = GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity,
													   ECC_Camera, FCollisionShape::MakeSphere(PickupSphereRadius), Params);

	if (bHit && Hit.GetActor())
	{
		DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 20.f, FColor::Yellow, false, 3.0f);
		DrawDebugBox(GetWorld(),
					 Hit.GetActor()->GetActorLocation(),
					 FVector(40.f),
					 FColor::Green, false, 3.0f, 0, 2.f);
		DrawDebugString(GetWorld(),
						Hit.GetActor()->GetActorLocation() + FVector(0.f, 0.f, 60.f),
						FString::Printf(TEXT("HIT: %s"), *Hit.GetActor()->GetName()),
						nullptr, FColor::Green, 3.0f, true);

		UE_LOG(LogTemp, Warning, TEXT("[TryPickup] HIT -> Actor=%s | Tag 체크 시작"),
			   *Hit.GetActor()->GetName());
	}
	else
	{
		DrawDebugSphere(GetWorld(), End, PickupSphereRadius, 12, FColor::Red, false, 3.0f);
		DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 3.0f, 0, 1.5f);
		UE_LOG(LogTemp, Warning, TEXT("[TryPickup] NO HIT (ECC_Camera, Range=%.0f)"), PickupRange);
		return false;
	}

	AActor *Target = Hit.GetActor();
	if (!Target)
		return false;

	if (ATransformation_actor *TransActor = Cast<ATransformation_actor>(Target))
	{
		if (!TransActor->bCanBePickedUp)
		{
			DrawDebugBox(GetWorld(), Target->GetActorLocation(), FVector(40.f), FColor::Orange, false, 3.0f, 0, 2.f);
			DrawDebugString(GetWorld(), Target->GetActorLocation() + FVector(0.f, 0.f, 80.f),
							TEXT("bCanBePickedUp = false"), nullptr, FColor::Orange, 3.0f, true);
			UE_LOG(LogTemp, Warning, TEXT("[TryPickup] REJECTED: bCanBePickedUp=false on %s"), *Target->GetName());
			return false;
		}
	}

	const bool bHasValidTag = PickupTags.ContainsByPredicate([Target](const FName &Tag)
															 { return Target->ActorHasTag(Tag); });
	if (!bHasValidTag)
	{
		DrawDebugBox(GetWorld(), Target->GetActorLocation(), FVector(40.f), FColor::Red, false, 3.0f, 0, 2.f);
		DrawDebugString(GetWorld(), Target->GetActorLocation() + FVector(0.f, 0.f, 80.f),
						TEXT("NO VALID TAG"), nullptr, FColor::Red, 3.0f, true);
		UE_LOG(LogTemp, Warning, TEXT("[TryPickup] REJECTED: 유효한 픽업 태그 없음 on %s"), *Target->GetName());
		return false;
	}

	DrawDebugBox(GetWorld(), Target->GetActorLocation(), FVector(45.f), FColor::Green, false, 3.0f, 0, 3.f);
	DrawDebugString(GetWorld(), Target->GetActorLocation() + FVector(0.f, 0.f, 100.f),
					TEXT("PICKUP OK!"), nullptr, FColor::Green, 3.0f, true);
	UE_LOG(LogTemp, Warning, TEXT("[TryPickup] SUCCESS: %s 픽업 시작"), *Target->GetName());

	SetPrimitiveComponentsPhysics(Target, false);
	PendingPickupActor = Target;

	float RelativeYaw = Target->GetActorRotation().Yaw - GetActorRotation().Yaw;
	RelativeYaw = FMath::RoundToFloat(RelativeYaw / 90.f) * 90.f;
	HeldRelativeQuat = FQuat(FRotator(0.f, RelativeYaw, 0.f));

	if (PickupAnim && GetMesh())
	{
		PlaySmoothAnim(PickupAnim, false);
		bIsPickingUp = true;
		GetWorld()->GetTimerManager().SetTimer(AttachmentTimerHandle, this,
											   &AmaterialCharacter::HandleActualAttachment, PickupAnimAttachTime, false);
		GetWorld()->GetTimerManager().SetTimer(PickupEndTimerHandle, this,
											   &AmaterialCharacter::OnPickupAnimFinished, PickupAnim->GetPlayLength(), false);
	}
	return true;
}

void AmaterialCharacter::HandleActualAttachment()
{
	if (!PendingPickupActor || !HoldPivot)
		return;
	CaptureHeldLocalExtent(PendingPickupActor);

	HeldActor = PendingPickupActor;
	PendingPickupActor = nullptr;

	if (ATransformation_actor *TransActor = Cast<ATransformation_actor>(HeldActor))
		TransActor->ClearPower();

	HeldActor->AttachToComponent(HoldPivot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	HeldActor->SetActorRelativeLocation(FVector::ZeroVector);

	FQuat DesiredWorldQuat = GetActorQuat() * HeldRelativeQuat;
	FQuat PivotWorldQuat = HoldPivot->GetComponentQuat();
	HeldActor->SetActorRelativeRotation((PivotWorldQuat.Inverse() * DesiredWorldQuat).Rotator());

	UpdateHoldPivotTransform();
	ApplyWeightSpeedPenalty(HeldActor);
}

void AmaterialCharacter::OnPickupAnimFinished()
{
	bIsPickingUp = false;

	GetWorld()->GetTimerManager().SetTimer(PickupEndTimerHandle, [this]()
										   {
				bWasHolding = false;
				bIsPlayingWalk = false;
				UpdateAnimation(); }, 0.2f, false);
}

void AmaterialCharacter::UpdateHeldActorPosition()
{
	if (!HeldActor)
		return;

	const float SizeRatio = FMath::Clamp(HeldLocalExtent.GetMax() / 50.f, 0.3f, 1.2f);
	const float DistanceScale = FMath::Lerp(0.6f, 1.05f, SizeRatio);
	const float AdjustedDistance = HoldDistance * DistanceScale;

	const FVector ForwardOffset = GetActorForwardVector() * AdjustedDistance;
	const FVector HeightOffset = FVector(0.f, 0.f, HoldHeight);
	const FVector TargetLocation = GetActorLocation() + ForwardOffset + HeightOffset;

	HeldActor->SetActorLocation(TargetLocation);
	HeldActor->SetActorRotation((GetActorQuat() * HeldRelativeQuat).Rotator());
}

void AmaterialCharacter::DropHeld()
{
	if (!HeldActor)
		return;
	if (!PickupAnim || !GetMesh())
		return;

	// ★ 물체 내릴 때 재질별 사운드 (1초 뒤 무작위 재생)
	if (HeldActor->ActorHasTag(Tag_Metal)  ||
		HeldActor->ActorHasTag(Tag_Ice)    ||
		HeldActor->ActorHasTag(Tag_Copper) ||
		HeldActor->ActorHasTag(Tag_Magnet))
	{
		ScheduleDropSound(GetRandomMetallicSound());
	}
	if (HeldActor->ActorHasTag(Tag_Rubber))
		ScheduleDropSound(GetRandomRubberSound());
	if (HeldActor->ActorHasTag(Tag_Wood))
		ScheduleDropSound(GetRandomWoodSound());

	if (HoldPivot)
	{
		HeldActor->AttachToComponent(HoldPivot, FAttachmentTransformRules::KeepWorldTransform);
	}

	// 픽업 애니메이션을 끝에서부터 역재생 (내려놓기 동작)
	PlaySmoothAnim(PickupAnim, false, -1.0f, PickupAnim->GetPlayLength());
	bIsPickingUp = true;

	FTimerHandle DropTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(DropTimerHandle, [this]()
										   {
				if (!HeldActor) return;

				HeldActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				HeldActor->SetActorLocation(HeldActor->GetActorLocation() +
					GetActorForwardVector() * DropForwardOffset);

				SetPrimitiveComponentsPhysics(HeldActor, true);
				ATransformation_actor* TransActor = Cast<ATransformation_actor>(HeldActor);
				RestoreWalkSpeed();
				HeldActor = nullptr; }, DropDetachTime, false);

	GetWorld()->GetTimerManager().SetTimer(PickupEndTimerHandle, [this]()
										   {
				bIsPickingUp   = false;
				bWasHolding    = false;
				bIsPlayingWalk = false;
				if (HoldPivot)
				{
					HoldPivot->SetRelativeLocation(FVector::ZeroVector);
					HoldPivot->SetRelativeRotation(FRotator::ZeroRotator);
				}
				UpdateAnimation(); }, PickupAnim->GetPlayLength(), false);
}

void AmaterialCharacter::ApplyWeightSpeedPenalty(AActor *Actor)
{
	if (!Actor)
		return;
	UCharacterMovementComponent *MoveComp = GetCharacterMovement();
	if (!MoveComp)
		return;
	OriginalMaxWalkSpeed = MoveComp->MaxWalkSpeed;
	MoveComp->MaxWalkSpeed = OriginalMaxWalkSpeed * 0.5f;
}

void AmaterialCharacter::RestoreWalkSpeed()
{
	if (OriginalMaxWalkSpeed <= 0.f)
		return;
	UCharacterMovementComponent *MoveComp = GetCharacterMovement();
	if (MoveComp)
		MoveComp->MaxWalkSpeed = OriginalMaxWalkSpeed;
	OriginalMaxWalkSpeed = 0.f;
}

void AmaterialCharacter::CaptureHeldLocalExtent(AActor *Actor)
{
	if (!Actor)
	{
		HeldLocalExtent = FVector(50.f);
		return;
	}
	FVector Origin, BoxExtent;
	Actor->GetActorBounds(false, Origin, BoxExtent);
	HeldLocalExtent = BoxExtent.IsNearlyZero() ? FVector(50.f) : BoxExtent;
}

void AmaterialCharacter::UpdateHoldPivotTransform()
{
	if (!HoldPivot)
		return;
	const bool bMoving = IsMoving();

	float ObjectScale = 0.4f;

	if (HeldActor)
	{
		if (UStaticMeshComponent *SMC = HeldActor->FindComponentByClass<UStaticMeshComponent>())
		{
			ObjectScale = SMC->GetComponentScale().GetMax();
		}
		else
		{
			ObjectScale = HeldActor->GetActorScale3D().GetMax();
		}
	}

	const float AdjustedY = FMath::GetMappedRangeValueClamped(
		FVector2D(-0.1f, 1.0f),
		FVector2D(-0.25f, -0.40f),
		ObjectScale);

	const float AdjustedZ = FMath::GetMappedRangeValueClamped(
		FVector2D(-0.1f, 1.0f),
		bMoving ? FVector2D(-0.005f, -0.01f) : FVector2D(0.05f, 0.10f),
		ObjectScale);

	FVector FinalLoc(-0.20f, AdjustedY, AdjustedZ);

	FinalLoc += bMoving ? HoldExtraLocalOffset_Walk : HoldExtraLocalOffset_Idle;
	HoldPivot->SetRelativeLocation(FinalLoc);
	HoldPivot->SetRelativeRotation(bMoving ? HoldLocalRot_Walk : HoldLocalRot_Idle);

	if (HeldActor)
	{
		FQuat DesiredWorldQuat = GetActorQuat() * HeldRelativeQuat;
		FQuat PivotWorldQuat = HoldPivot->GetComponentQuat();
		HeldActor->SetActorRelativeRotation((PivotWorldQuat.Inverse() * DesiredWorldQuat).Rotator());
	}
}

void AmaterialCharacter::UpdateAnimation()
{
	if (bIsPickingUp || !GetMesh())
		return;
	const bool bMoving = IsMoving();
	const bool bHolding = (HeldActor != nullptr);
	if (bWasHolding != bHolding || bMoving != bIsPlayingWalk)
	{
		PlayAnimIfValid(GetAnimForState(bMoving, bHolding), true);

		bWasHolding = bHolding;
		bIsPlayingWalk = bMoving;
	}
}

UAnimSequence *AmaterialCharacter::GetAnimForState(bool bMoving, bool bHolding) const
{
	if (bMoving)
		return bHolding ? WalkBringAnim : WalkAnim;
	return bHolding ? IdleBringAnim : IdleAnim;
}

void AmaterialCharacter::PlayAnimIfValid(UAnimSequence *Anim, bool bLooping) const
{
	PlaySmoothAnim(Anim, bLooping);
}

void AmaterialCharacter::PlaySmoothAnim(UAnimSequence *Anim, bool bLooping, float PlayRate, float StartPosition) const
{
	if (!Anim || !GetMesh())
		return;

	if (UmaterialAnimInstance *AnimInst = Cast<UmaterialAnimInstance>(GetMesh()->GetAnimInstance()))
		AnimInst->PlayAnimationSmooth(Anim, bLooping, AnimBlendTime, PlayRate, StartPosition);
	else
		GetMesh()->PlayAnimation(Anim, bLooping); // 커스텀 인스턴스가 없으면 기존 방식
}

void AmaterialCharacter::SetPrimitiveComponentsPhysics(AActor *Actor, bool bEnable) const
{
	if (!Actor)
		return;
	TArray<UPrimitiveComponent *> PrimComps;
	Actor->GetComponents<UPrimitiveComponent>(PrimComps);
	for (UPrimitiveComponent *PC : PrimComps)
	{
		if (PC)
		{
			if (bEnable)
			{
				PC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				PC->SetSimulatePhysics(true);
				PC->SetEnableGravity(true);
			}
			else
			{
				PC->SetSimulatePhysics(false);
				PC->SetEnableGravity(false);
				PC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}
}

void AmaterialCharacter::OnWindowFocusChanged(bool bHasFocus)
{
	if (!bHasFocus)
	{
		bHadFocusBefore = true;
	}
	else if (bHasFocus && bHadFocusBefore && bMouseCaptured && !bGamePaused)
	{
		APlayerController *PC = Cast<APlayerController>(GetController());
		if (PC)
		{
			PC->bShowMouseCursor = true;
			FInputModeGameAndUI InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(InputMode);
			bMouseCaptured = false;
		}
	}
}

void AmaterialCharacter::OnEscapePressed()
{
	static bool bIsProcessing = false;
	if (bIsProcessing)
		return;
	bIsProcessing = true;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		bIsProcessing = false;
		return;
	}

	if (!bGamePaused)
		OpenPauseMenu();
	else
		ClosePauseMenu();

	bIsProcessing = false;
}

const int32* AmaterialCharacter::GetGaugeRefByTag(const FName &MaterialTag) const
{
	if (MaterialTag == Tag_Rubber) return &RubberGauge;
	if (MaterialTag == Tag_Metal)  return &MetalGauge;
	if (MaterialTag == Tag_Copper) return &CopperGauge;
	if (MaterialTag == Tag_Ice)    return &IceGauge;
	if (MaterialTag == Tag_Wood)   return &WoodGauge;
	if (MaterialTag == Tag_Magnet) return &MagnetGauge;
	return nullptr;
}

int32* AmaterialCharacter::GetGaugeRefByTag(const FName &MaterialTag)
{
	return const_cast<int32*>(static_cast<const AmaterialCharacter*>(this)->GetGaugeRefByTag(MaterialTag));
}

void AmaterialCharacter::DecreaseGaugeForMaterial(const FName &MaterialTag)
{
	UE_LOG(LogTemp, Warning, TEXT("DecreaseGauge: %s"), *MaterialTag.ToString());

	if (MaterialTag == Tag_Joker)
		return;

	if (int32* Gauge = GetGaugeRefByTag(MaterialTag))
		*Gauge = FMath::Clamp(*Gauge - GaugeDecreaseAmount, 0, MaxGauge);
}

int32 AmaterialCharacter::GetGaugeByTag(const FName &MaterialTag) const
{
	const int32* Gauge = GetGaugeRefByTag(MaterialTag);
	return Gauge ? *Gauge : 0;
}

void AmaterialCharacter::ChargeGaugeForMaterial(const FName &MaterialTag, int32 Amount)
{
	if (Amount <= 0)
		return;

	if (int32* Gauge = GetGaugeRefByTag(MaterialTag))
		*Gauge = FMath::Clamp(*Gauge + Amount, 0, MaxGauge);

	UE_LOG(LogTemp, Warning, TEXT("ChargeGauge: %s +%d"), *MaterialTag.ToString(), Amount);
}

void AmaterialCharacter::ResetAllGauges()
{
	RubberGauge = 0;
	MetalGauge  = 0;
	IceGauge    = 0;
	WoodGauge   = 0;
	MagnetGauge = 0;
	CopperGauge = 0;

	UE_LOG(LogTemp, Warning, TEXT("[Gauge] 모든 게이지 리셋 (0)"));
}

void AmaterialCharacter::OpenRadialMenu(ATransformation_actor *Target)
{
	APlayerController *PC = Cast<APlayerController>(GetController());
	if (!PC)
		return;

	if (!RadialMenuWidget && RadialMenuClass)
	{
		RadialMenuWidget = CreateWidget<UTestUI>(PC, RadialMenuClass);
		RadialMenuWidget->AddToViewport(100);
		RadialMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (!RadialMenuWidget)
		return;

	RadialMenuWidget->TargetActor = nullptr;
	RadialMenuWidget->SetVisibility(ESlateVisibility::Visible);

	PC->bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(InputMode);

	bRadialMenuOpen = true;
	bMouseCaptured = false;
}

void AmaterialCharacter::CloseRadialMenu(bool bConfirm)
{
	if (!RadialMenuWidget)
		return;

	RadialMenuWidget->SetVisibility(ESlateVisibility::Collapsed);

	if (APlayerController *PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		bMouseCaptured = true;
	}

	if (bConfirm)
	{
		EBlockForm SelForm;
		if (RadialMenuWidget->GetSelectedForm(SelForm))
		{
			LoadedForm = SelForm;
			bHasLoadedForm = true;
			UE_LOG(LogTemp, Warning, TEXT("[RadialMenu] 머터리얼 장전: %d"), (int32)SelForm);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[RadialMenu] 데드존/미선택 - 장전 안 됨"));
		}
	}

	bRadialMenuOpen = false;
	UpdateAnimation();
}

void AmaterialCharacter::OnUseEAnimFinished()
{
	bIsPickingUp = false;
	UpdateAnimation();
	OpenRadialMenu(nullptr);
}

void AmaterialCharacter::OnUseLeftAnimFinished()
{
	if (PendingShotTarget)
	{
		ApplyLoadedFormTo(PendingShotTarget);
		PendingShotTarget = nullptr;
	}

	bRadialMenuOpen = false;
	bIsPickingUp = false;
	bWasHolding = false;
	bIsPlayingWalk = false;
	UpdateAnimation();
}

void AmaterialCharacter::WarpToLevel(const FString &LevelPath)
{
	UGameplayStatics::OpenLevel(GetWorld(), FName(*LevelPath));
}

void AmaterialCharacter::OnWarpLaboratory()
{
	WarpToLevel(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
}

void AmaterialCharacter::OnWarpStage1()
{
	WarpToLevel(TEXT("/Game/stage/Stage1/Stage1"));
}

void AmaterialCharacter::OnWarpStage2()
{
	const FVector  DestLocation = FVector(5020.0, 37394.0, 290.0);
	const FString  DestLevel    = TEXT("/Game/stage/MainStage/MainStage1");

	// PIE 접두사(UEDPIE_0_)를 정확히 제거
	const FString CleanCurrent = UWorld::RemovePIEPrefix(GetWorld()->GetMapName());

	FString DestMapName = DestLevel;
	int32 SlashIdx;
	if (DestLevel.FindLastChar('/', SlashIdx))
		DestMapName = DestLevel.RightChop(SlashIdx + 1);

	UE_LOG(LogTemp, Warning, TEXT("[Warp] Current='%s' Dest='%s'"), *CleanCurrent, *DestMapName);

	if (CleanCurrent.Equals(DestMapName, ESearchCase::IgnoreCase))
	{
		SetActorLocation(DestLocation, false, nullptr, ETeleportType::TeleportPhysics);
		UE_LOG(LogTemp, Warning, TEXT("[Warp] 같은 맵 - 순간이동만"));
	}
	else
	{
		PendingSpawnLocation = DestLocation;
		bHasPendingSpawn = true;
		WarpToLevel(DestLevel);
		UE_LOG(LogTemp, Warning, TEXT("[Warp] 다른 맵 - 레벨 로드"));
	}
}

void AmaterialCharacter::OnWarpStage3()
{
	WarpToLevel(TEXT("/Game/stage/Stage3/Stage3"));
}

void AmaterialCharacter::UseSyringePressed()
{
	if (!AttachedSyringe || bIsUsingSyringe || bIsPickingUp)
		return;

	if (InsertAnim && GetMesh())
	{
		PlaySmoothAnim(InsertAnim, false);
		bIsUsingSyringe = true;
		bIsPickingUp = true;

		AttachedSyringe->StartRotationAnim();

		GetWorld()->GetTimerManager().SetTimer(RadialMenuAnimTimer, this,
											   &AmaterialCharacter::OnInsertAnimFinished, InsertAnim->GetPlayLength(), false);
	}
}

void AmaterialCharacter::OnInsertAnimFinished()
{
	bIsUsingSyringe = false;
	bIsPickingUp = false;

	if (AttachedSyringe)
	{
		AttachedSyringe->UseSyringe(this);
		AttachedSyringe->Destroy();
		AttachedSyringe = nullptr;
	}

	UpdateAnimation();
}

void AmaterialCharacter::UpdateHeldMagnetism()
{
	if (!HeldActor)
		return;

	const bool bHoldingMagnet = HeldActor->ActorHasTag(Tag_Magnet);
	const bool bHoldingMetal = HeldActor->ActorHasTag(Tag_Metal);
	if (!bHoldingMagnet && !bHoldingMetal)
		return;

	ATransformation_actor *HeldMagnet = bHoldingMagnet ? Cast<ATransformation_actor>(HeldActor) : nullptr;

// ★ 들고 있는 자석이 소자(Curie)된 상태면 자력 0 → 아무것도 끌지 않음
	if (HeldMagnet && HeldMagnet->IsDemagnetized())
    return;

const FVector Center = GetActorLocation();

	const float SizeScale = bHoldingMetal
								? FMath::Clamp(HeldLocalExtent.GetMax() / 50.f, 0.5f, 2.0f)
								: 1.0f;

	DrawDebugSphere(GetWorld(), Center, MagnetScanRange, 16, FColor::Blue, false, 0.f);
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Cyan,
										 FString::Printf(TEXT("들고 있음: %s | 스캔 범위: %.0f | SizeScale: %.2f"),
														 bHoldingMagnet ? TEXT("Magnet") : TEXT("Metal"), MagnetScanRange, SizeScale));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HeldMagnetism), false, this);
	Params.AddIgnoredActor(HeldActor);

	TArray<FOverlapResult> Hits;
	GetWorld()->OverlapMultiByObjectType(Hits, Center, FQuat::Identity,
										 FCollisionObjectQueryParams::AllObjects,
										 FCollisionShape::MakeSphere(MagnetScanRange), Params);

	int32 MetalCount = 0;
	int32 MagnetCount = 0;

	for (const FOverlapResult &Hit : Hits)
	{
		AActor *OtherActor = Hit.GetActor();
		if (!OtherActor || OtherActor == this)
			continue;

		UPrimitiveComponent *PrimComp = Hit.GetComponent();
		if (!PrimComp)
			continue;
		if (!PrimComp->IsSimulatingPhysics())
		{
			if (OtherActor->ActorHasTag(Tag_Magnet))
				PrimComp->WakeRigidBody();
			else
				continue;
		}

		const FVector OtherLoc = OtherActor->GetActorLocation();
		const FVector ToCenter = Center - OtherLoc;
		const float Dist = ToCenter.Size();
		if (Dist < 1.f)
			continue;

		const FVector Dir = ToCenter / Dist;
		const float SafeDist = FMath::Max(Dist, 10.f);

		if (bHoldingMagnet)
		{
			if (OtherActor->ActorHasTag(Tag_Metal))
			{
				float ForceMag = MagnetForceStrength * SizeScale * 800.f;
				ForceMag *= FMath::Clamp(1.f - (SafeDist / MagnetScanRange), 0.1f, 1.f);

				const FVector CurVel = PrimComp->GetPhysicsLinearVelocity();
				const float VelToward = FVector::DotProduct(CurVel, Dir);
				if (VelToward > MagnetMaxVelocity * 0.5f)
					ForceMag *= FMath::Clamp(1.f - (VelToward / MagnetMaxVelocity), 0.1f, 1.f);

				PrimComp->AddForce(Dir * ForceMag, NAME_None, false);

				DrawDebugLine(GetWorld(), Center, OtherLoc, FColor::Green, false, 0.f, 0, 2.f);
				DrawDebugString(GetWorld(), OtherLoc + FVector(0, 0, 40.f),
								FString::Printf(TEXT("[ATTRACT] Metal | dist:%.0f | force:%.0f"), Dist, ForceMag),
								nullptr, FColor::Green, 0.f, true);

				MetalCount++;
			}
			else if (OtherActor->ActorHasTag(Tag_Magnet))
			{
				ATransformation_actor *OtherMagnet = Cast<ATransformation_actor>(OtherActor);
				if (!OtherMagnet || !HeldMagnet)
        			continue;

    		// ★ 대상 자석이 소자됐으면 끌지도 밀지도 않음
    			if (OtherMagnet->IsDemagnetized())
        			continue;

				const FVector MyNorth = HeldMagnet->GetNorthPoleWorldDir();
				const FVector DirToOther = -Dir;
				const FVector OtherNorth = OtherMagnet->GetNorthPoleWorldDir();

				const float MyPole = FVector::DotProduct(MyNorth, DirToOther);
				const float OtherPole = FVector::DotProduct(OtherNorth, -DirToOther);
				const float Polarity = -(MyPole * OtherPole);

				float ForceMag = (MagnetForceStrength * 1500.f) / SafeDist;
				ForceMag *= PrimComp->GetMass();

				const FVector ForceDir = Dir * FMath::Sign(Polarity);

				const FVector CurVel = PrimComp->GetPhysicsLinearVelocity();
				const float VelToward = FVector::DotProduct(CurVel, ForceDir);
				if (VelToward > MagnetMaxVelocity * 0.5f)
					ForceMag *= FMath::Clamp(1.f - (VelToward / MagnetMaxVelocity), 0.1f, 1.f);

				PrimComp->AddForce(ForceDir * ForceMag, NAME_None, false);

				const FColor LineColor = Polarity > 0.f ? FColor::Green : FColor::Red;
				const FString Label = Polarity > 0.f ? TEXT("[ATTRACT]") : TEXT("[REPEL]");
				DrawDebugLine(GetWorld(), Center, OtherLoc, LineColor, false, 0.f, 0, 2.f);
				DrawDebugString(GetWorld(), OtherLoc + FVector(0, 0, 40.f),
								FString::Printf(TEXT("%s Magnet | dist:%.0f | force:%.0f | polarity:%.2f"),
												*Label, Dist, ForceMag, Polarity),
								nullptr, LineColor, 0.f, true);

				MagnetCount++;
			}
		}
		else if (bHoldingMetal)
		{
			if (!OtherActor->ActorHasTag(Tag_Magnet))
				continue;

			ATransformation_actor *OtherMagnet = Cast<ATransformation_actor>(OtherActor);
			if (!OtherMagnet)
				continue;

    		if (OtherMagnet->IsDemagnetized())
        	continue;

			float ForceMag = MagnetForceStrength * SizeScale * 300000.f;
			ForceMag *= FMath::Clamp(1.f - (SafeDist / MagnetScanRange), 0.1f, 1.f);

			PrimComp->WakeRigidBody();
			PrimComp->AddImpulse(Dir * 40.f, NAME_None, true);

			DrawDebugLine(GetWorld(), Center, OtherLoc, FColor::Green, false, 0.f, 0, 2.f);
			DrawDebugString(GetWorld(), OtherLoc + FVector(0, 0, 40.f),
							FString::Printf(TEXT("[ATTRACT] Magnet | dist:%.0f | force:%.0f"), Dist, ForceMag),
							nullptr, FColor::Green, 0.f, true);

			MagnetCount++;
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(2, 0.f, FColor::Green,
										 FString::Printf(TEXT("감지된 Metal: %d개"), MetalCount));
		GEngine->AddOnScreenDebugMessage(3, 0.f, FColor::Magenta,
										 FString::Printf(TEXT("감지된 Magnet: %d개"), MagnetCount));
	}
}

void AmaterialCharacter::FireMaterialShot()
{
	if (!bHasLoadedForm) return;
	if (bIsPickingUp || bIsUsingSyringe) return;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	FVector  Loc;
	FRotator Rot;
	PC->GetPlayerViewPoint(Loc, Rot);

	const FVector Start = Loc + FRotationMatrix(Rot).GetUnitAxis(EAxis::Y) * -15.f;
	const FVector End   = Start + Rot.Vector() * InteractRange;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MaterialShot), false, this);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	DrawDebugLine(GetWorld(), Start, bHit ? Hit.ImpactPoint : End, bHit ? FColor::Green : FColor::Red, false, 2.0f, 0, 2.0f);

	if (!bHit) return;

	ATransformation_actor* TransformActor = Cast<ATransformation_actor>(Hit.GetActor());
	if (!TransformActor) return;

	PendingShotTarget = TransformActor;

	if (UseLeftAnim && GetMesh())
	{
		PlaySmoothAnim(UseLeftAnim, false);
		bIsPickingUp = true;
		FTimerHandle ShotApplyHandle;
		GetWorld()->GetTimerManager().SetTimer(ShotApplyHandle, [this]()
		{
			if (ShootSound) UGameplayStatics::PlaySound2D(this, ShootSound);
			if (PendingShotTarget) { ApplyLoadedFormTo(PendingShotTarget); PendingShotTarget = nullptr; }
		}, 0.3f, false);
		GetWorld()->GetTimerManager().SetTimer(RadialMenuAnimTimer, this,
			&AmaterialCharacter::OnUseLeftAnimFinished, UseLeftAnim->GetPlayLength(), false);
	}
	else
	{
		ApplyLoadedFormTo(PendingShotTarget);
		PendingShotTarget = nullptr;
	}
}

FName AmaterialCharacter::FormToTag(EBlockForm Form)
{
	switch (Form)
	{
	case EBlockForm::Rubber: return TEXT("Rubber");
	case EBlockForm::Metal:  return TEXT("Metal");
	case EBlockForm::Ice:    return TEXT("Ice");
	case EBlockForm::Wood:   return TEXT("Wood");
	case EBlockForm::Magnet: return TEXT("Magnet");
	case EBlockForm::Copper: return TEXT("Copper");
	default:                 return NAME_None;
	}
}

void AmaterialCharacter::ApplyLoadedFormTo(ATransformation_actor *Target)
{
    if (!Target || !bHasLoadedForm)
        return;

    if (!Target->bCanChangeForm)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MaterialShot] 잠긴 블럭 - 게이지 소모 안 함"));
        return;
    }

    if (Target->GetCurrentForm() == LoadedForm)
        return;

    const FName Tag = FormToTag(LoadedForm);
    if (Tag.IsNone())
        return;

    Target->SetForm(LoadedForm);
    DecreaseGaugeForMaterial(Tag);
    UE_LOG(LogTemp, Warning, TEXT("[MaterialShot] 적용 %s -> %s"), *Target->GetName(), *Tag.ToString());
}

void AmaterialCharacter::UpdateGroundFriction()
{
    UCharacterMovementComponent* Move = GetCharacterMovement();
    if (!Move) return;

    bool bOnIce = false;

    if (Move->IsMovingOnGround())
    {
        const FHitResult& Floor = Move->CurrentFloor.HitResult;
        if (AActor* FloorActor = Floor.GetActor())
        {
            bOnIce = FloorActor->ActorHasTag(Tag_Ice);
        }
    }

    if (bOnIce != bWasOnIce)
    {
        if (bOnIce)
        {
            Move->GroundFriction             = IceGroundFriction;
            Move->BrakingDecelerationWalking = IceBrakingDeceleration;
        }
        else
        {
            Move->GroundFriction             = DefaultGroundFriction;
            Move->BrakingDecelerationWalking = DefaultBrakingDeceleration;
        }
        bWasOnIce = bOnIce;
    }
}

void AmaterialCharacter::DoRubberBounce(const FVector& SurfaceNormal)
{
    UCharacterMovementComponent* Move = GetCharacterMovement();
    if (!Move) return;

    const FVector N = SurfaceNormal.GetSafeNormal();
    const FVector Vel = GetVelocity();

    const float VIntoSurface = FVector::DotProduct(Vel, -N);

    if (VIntoSurface < RubberPlayerStopThreshold)
        return;   // 안 튕길 만큼 느리면 소리도 안 냄

    // ★ 실제로 튕기는 순간 고무 사운드 (무작위)
    if (USoundBase* Snd = GetRandomRubberSound())
        UGameplayStatics::PlaySoundAtLocation(this, Snd, GetActorLocation());

    FVector Bounced = Vel + (1.f + RubberPlayerRestitution) * VIntoSurface * N;

    float AlongN = FVector::DotProduct(Bounced, N);
    const float MinBounce = VIntoSurface * RubberPlayerRestitution;
    if (AlongN < MinBounce)
        Bounced += N * (MinBounce - AlongN);

    Bounced = Bounced.GetClampedToMaxSize(RubberPlayerMaxBounce);

    LaunchCharacter(Bounced, true, true);
}

void AmaterialCharacter::Landed(const FHitResult& Hit)
{
    Super::Landed(Hit);

    // ★ 착지 시 재질별 사운드 (쿨다운 포함)
    TryPlayImpactSound(Hit.GetActor(), Hit.ImpactPoint);

    ATransformation_actor* Block = Cast<ATransformation_actor>(Hit.GetActor());
    if (!Block || Block->GetCurrentForm() != EBlockForm::Rubber)
        return;

    DoRubberBounce(Hit.ImpactNormal);
}

void AmaterialCharacter::OnCapsuleHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    // ★ 부딪친 재질별 사운드 (쿨다운으로 연속 충돌 시 중복 방지)
    TryPlayImpactSound(OtherActor, Hit.ImpactPoint);

    ATransformation_actor* Block = Cast<ATransformation_actor>(OtherActor);
    if (!Block || Block->GetCurrentForm() != EBlockForm::Rubber)
        return;

    DoRubberBounce(Hit.ImpactNormal);
}

void AmaterialCharacter::OnResetMap()
{
    // 1. 현재 위치에서 Z축으로 +5.0f (언리얼 단위로 5cm) 올린 좌표를 저장합니다.
    // (만약 캡슐 콜리전 때문에 바닥에 끼인다면 50.0f 정도로 늘려주시면 좋습니다.)
    PendingSpawnLocation = GetActorLocation() + FVector(0.f, 0.f, 5.0f);
    bHasPendingSpawn = true;

    // 2. 현재 플레이 중인 맵의 이름을 가져옵니다. (에디터 플레이 시 붙는 접두사 제거)
    const FString CleanCurrentMapName = UWorld::RemovePIEPrefix(GetWorld()->GetMapName());

    UE_LOG(LogTemp, Warning, TEXT("[Reset] 현재 맵(%s)을 리셋합니다. 스폰 예정 위치: %s"), 
           *CleanCurrentMapName, *PendingSpawnLocation.ToString());

    // 3. 해당 맵을 다시 로드하여 맵 상태를 초기화합니다.
    UGameplayStatics::OpenLevel(GetWorld(), FName(*CleanCurrentMapName));
}

void AmaterialCharacter::OpenPauseMenu()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
		return;

	if (!PauseMenuWidget && PauseMenuWidgetClass)
		PauseMenuWidget = CreateWidget<UUserWidget>(PC, PauseMenuWidgetClass);

	if (PauseMenuWidget && !PauseMenuWidget->IsInViewport())
		PauseMenuWidget->AddToViewport(50);

	PC->SetPause(true);
	PC->bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(InputMode);

	bGamePaused = true;
	bMouseCaptured = false;
}

void AmaterialCharacter::ClosePauseMenu()
{
	if (PauseMenuWidget)
		PauseMenuWidget->RemoveFromParent();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetPause(false);
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
	}

	// ★★ 이 두 줄 빠뜨리면 Resume 누른 뒤 첫 ESC가 먹통됨 (상태 꼬임)
	bGamePaused = false;
	bMouseCaptured = true;
}

void AmaterialCharacter::HideCrosshair()
{
    if (CrosshairWidget)
        CrosshairWidget->SetVisibility(ESlateVisibility::Hidden);
}