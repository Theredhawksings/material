#include "materialCharacter.h"
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
	#include "TimerManager.h"
	#include "Framework/Application/SlateApplication.h"
	#include "Kismet/GameplayStatics.h"
	#include "Engine/OverlapResult.h"

	AmaterialCharacter::AmaterialCharacter()
		: HeldActor(nullptr)
		, PendingPickupActor(nullptr)
		, bIsPlayingWalk(false)
		, bWasHolding(false)
		, bIsPickingUp(false)
	{
		PrimaryActorTick.bCanEverTick = true;
		AutoPossessPlayer = EAutoReceiveInput::Player0;

		bUseControllerRotationPitch = false;
		bUseControllerRotationYaw = false;
		bUseControllerRotationRoll = false;

		UCharacterMovementComponent* Movement = GetCharacterMovement();
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.f, RotationRate, 0.f);
		Movement->JumpZVelocity = JumpVelocity;
		Movement->AirControl = AirControl;

		CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
		CameraBoom->SetupAttachment(RootComponent);
		CameraBoom->TargetArmLength = CameraArmLength;
		CameraBoom->SocketOffset = FVector(0.f, 0.f, CameraSocketOffsetZ);
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
			USkeletalMeshComponent* MeshComp = GetMesh();
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

		HoldPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HoldPivot"));
		HoldPivot->SetupAttachment(GetMesh());

		BackpackComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackpackComp"));
		BackpackComp->SetupAttachment(GetMesh());
		BackpackComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		static ConstructorHelpers::FObjectFinder<UStaticMesh> BackpackMeshAsset(
			TEXT("StaticMesh'/Game/modeling/Character/backPack/BackPack_final.BackPack_final'"));
		if (BackpackMeshAsset.Succeeded())
			BackpackComp->SetStaticMesh(BackpackMeshAsset.Object);

		BackpackUIComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("BackpackUIComp"));
		BackpackUIComp->SetupAttachment(BackpackComp); 
		BackpackUIComp->SetWidgetSpace(EWidgetSpace::World);
		BackpackUIComp->SetTwoSided(true);
		BackpackUIComp->SetDrawAtDesiredSize(true);

		static ConstructorHelpers::FClassFinder<UUserWidget> BackpackUIBP(
			TEXT("/Game/modeling/Character/backPack/WBP_BackpackUI"));
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

		struct FAnimLoader { const TCHAR* Path; TObjectPtr<UAnimSequence>* Target; };
		const FAnimLoader AnimAssets[] = {
			{ TEXT("AnimSequence'/Game/modeling/Animation/Walk1.Walk1'"),             &WalkAnim },
			{ TEXT("AnimSequence'/Game/modeling/Animation/Test.Test'"),               &IdleAnim },
			{ TEXT("AnimSequence'/Game/modeling/Animation/bring.bring'"),             &PickupAnim }, 
			{ TEXT("AnimSequence'/Game/modeling/Animation/idle_bring2.idle_bring2'"), &IdleBringAnim },
			{ TEXT("AnimSequence'/Game/modeling/Animation/Walk_bring1.Walk_bring1'"), &WalkBringAnim },
			{ TEXT("AnimSequence'/Game/modeling/Animation/Use_E.Use_E'"),             &UseEAnim },
			{ TEXT("AnimSequence'/Game/modeling/Animation/Use_Left.Use_Left'"),       &UseLeftAnim },
		};

		for (const FAnimLoader& Loader : AnimAssets)
		{
			ConstructorHelpers::FObjectFinder<UAnimSequence> AnimAsset(Loader.Path);
			if (AnimAsset.Succeeded()) *Loader.Target = AnimAsset.Object;
		}

		static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC_DefaultAsset(
			TEXT("InputMappingContext'/Game/Input/IMC_Default.IMC_Default'"));
		if (IMC_DefaultAsset.Succeeded()) IMC_Default = IMC_DefaultAsset.Object;

		static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC_MouseLookAsset(
			TEXT("InputMappingContext'/Game/Input/IMC_MouseLook.IMC_MouseLook'"));
		if (IMC_MouseLookAsset.Succeeded()) IMC_MouseLook = IMC_MouseLookAsset.Object;

		struct FActionLoader { const TCHAR* Path; TObjectPtr<UInputAction>* Target; };
		const FActionLoader InputActions[] = {
			{ TEXT("InputAction'/Game/Input/Actions/IA_Move.IA_Move'"),           &IA_Move },
			{ TEXT("InputAction'/Game/Input/Actions/IA_Look.IA_Look'"),           &IA_Look },
			{ TEXT("InputAction'/Game/Input/Actions/IA_MouseLook.IA_MouseLook'"), &IA_MouseLook },
			{ TEXT("InputAction'/Game/Input/Actions/IA_Jump.IA_Jump'"),           &IA_Jump },
			{ TEXT("InputAction'/Game/Input/Actions/IA_LeftClick.IA_LeftClick'"), &IA_LeftClick },
			{ TEXT("InputAction'/Game/Input/Actions/IA_Escape.IA_Escape'"),       &IA_Escape },
		};
		for (const FActionLoader& Loader : InputActions)
		{
			ConstructorHelpers::FObjectFinder<UInputAction> ActionAsset(Loader.Path);
			if (ActionAsset.Succeeded()) *Loader.Target = ActionAsset.Object;
		}

		if (PickupTags.Num() == 0)
			PickupTags = { TEXT("Metal"), TEXT("Copper"), TEXT("Rubber"), TEXT("Ice"), TEXT("Wood"), TEXT("Magnet") };

		static ConstructorHelpers::FObjectFinder<UMaterial> PlayerMat(
			TEXT("/Script/Engine.Material'/Game/modeling/Character/M_Character.M_Character'"));
		if (PlayerMat.Succeeded()) GetMesh()->SetMaterial(0, PlayerMat.Object);
	}

	void AmaterialCharacter::BeginPlay()
	{
		Super::BeginPlay();

		if (const APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				if (IMC_Default)   Subsystem->AddMappingContext(IMC_Default, 0);
				if (IMC_MouseLook) Subsystem->AddMappingContext(IMC_MouseLook, 1);
			}
		}

		USkeletalMeshComponent* MeshComp = GetMesh();
		if (!MeshComp) return;

		MeshComp->SetRenderCustomDepth(false);
		MeshComp->SetCustomDepthStencilValue(0);
		MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
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
			BackpackUIComp->SetRelativeLocation(FVector(0.f, -0.47f, 0.42f));
			BackpackUIComp->SetRelativeRotation(FRotator(0.f, 270.f, 0.f));
			BackpackUIComp->SetRelativeScale3D(FVector(0.016f, 0.016f, 0.020f));
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
				ArmSocketName2
			);
			ArmComp2->SetRelativeLocation(FVector(-0.031129f, 0.025846f, 0.032451f));
			ArmComp2->SetRelativeRotation(FRotator(0.701087f, 90.0f, 103.319739f));
			ArmComp2->SetRelativeScale3D(FVector(0.01f, 0.01f, 0.01f));
		}

		FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(
			this, &AmaterialCharacter::OnWindowFocusChanged);
	}

	void AmaterialCharacter::Tick(float DeltaTime)
	{
		Super::Tick(DeltaTime);

		if (HeldActor)
		{
			if (bIsPickingUp)
				UpdateHoldPivotTransform();
			else
				UpdateHeldActorPosition();
		}
		
		UpdateAnimation();
	}

	void AmaterialCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
	{
		Super::SetupPlayerInputComponent(PlayerInputComponent);

		PlayerInputComponent->BindAction("ChangeForm",  IE_Pressed, this, &AmaterialCharacter::ChangeForm);
		PlayerInputComponent->BindAction("Hold",        IE_Pressed, this, &AmaterialCharacter::HoldPressed);
		PlayerInputComponent->BindAction("Checkweight", IE_Pressed, this, &AmaterialCharacter::CheckWeight);
		PlayerInputComponent->BindAction("laboratory",  IE_Pressed, this, &AmaterialCharacter::OnWarpLaboratory);
		PlayerInputComponent->BindAction("Stage1",      IE_Pressed, this, &AmaterialCharacter::OnWarpStage1);
		PlayerInputComponent->BindAction("Stage2",      IE_Pressed, this, &AmaterialCharacter::OnWarpStage2);
		PlayerInputComponent->BindAction("Stage3",      IE_Pressed, this, &AmaterialCharacter::OnWarpStage3);	

		UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
		if (!EIC) return;

		if (IA_Move)      EIC->BindAction(IA_Move,      ETriggerEvent::Triggered, this, &AmaterialCharacter::Move);
		if (IA_Look)      EIC->BindAction(IA_Look,      ETriggerEvent::Triggered, this, &AmaterialCharacter::Look);
		if (IA_MouseLook) EIC->BindAction(IA_MouseLook, ETriggerEvent::Triggered, this, &AmaterialCharacter::Look);

		if (IA_Jump)
		{
			EIC->BindAction(IA_Jump, ETriggerEvent::Started,   this, &AmaterialCharacter::JumpStarted);
			EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AmaterialCharacter::JumpStopped);
		}
		
		if (IA_LeftClick) EIC->BindAction(IA_LeftClick, ETriggerEvent::Started, this, &AmaterialCharacter::OnLeftClick);
		if (IA_Escape)    EIC->BindAction(IA_Escape,    ETriggerEvent::Started, this, &AmaterialCharacter::OnEscapePressed);
	}

	void AmaterialCharacter::Move(const FInputActionValue& Value)
	{
		if (!Controller || bIsPickingUp) return;
		const FVector2D Axis = Value.Get<FVector2D>();
		if (Axis.IsNearlyZero()) return;
		const FRotationMatrix RotMatrix(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f));
		AddMovementInput(RotMatrix.GetUnitAxis(EAxis::X), Axis.Y);
		AddMovementInput(RotMatrix.GetUnitAxis(EAxis::Y), Axis.X);
	}

	void AmaterialCharacter::Look(const FInputActionValue& Value)
	{
		const FVector2D Axis = Value.Get<FVector2D>();
		AddControllerYawInput(Axis.X);
		AddControllerPitchInput(Axis.Y);
	}

	void AmaterialCharacter::JumpStarted() { Jump(); }
	void AmaterialCharacter::JumpStopped() { StopJumping(); }

	void AmaterialCharacter::ChangeForm()
	{
		if (bIsPickingUp || HeldActor) return;

		if (bRadialMenuOpen)
		{
			CloseRadialMenu(false);
			return;
		}

		if (UseEAnim && GetMesh())
		{
			GetMesh()->PlayAnimation(UseEAnim, false);
			bIsPickingUp = true;
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
		if (!FollowCamera) return;
		const FVector Start = FollowCamera->GetComponentLocation();
		const FVector End   = Start + FollowCamera->GetForwardVector() * InteractRange;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CheckWeight), false, this);
		if (!GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity,
			ECC_Visibility, FCollisionShape::MakeSphere(InteractSphereRadius), Params)) return;
		AActor* HitActor = Hit.GetActor();
		if (!HitActor) return;
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(HitActor->GetRootComponent());
		if (!PrimComp) return;
		const float MassKg = PrimComp->GetMass();
		DrawDebugString(GetWorld(), HitActor->GetActorLocation() + FVector(0.f, 0.f, 80.f),
			FString::Printf(TEXT("%.1f kg"), MassKg), nullptr, FColor::Cyan, 3.0f, true);
	}

	void AmaterialCharacter::HoldPressed()
	{
		if (bIsPickingUp || bRadialMenuOpen) return;

		if (HeldActor) DropHeld();
		else TryPickup();
	}

	void AmaterialCharacter::OnLeftClick()
	{
		if(bRadialMenuOpen)
		{
			CloseRadialMenu(true);
			return;
		}

		static bool bIsProcessing = false;
		if (bIsProcessing) return;  
		bIsProcessing = true;
		
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (!PC) 
		{
			bIsProcessing = false;
			return;
		}
		
		if (bGamePaused)
		{
			PC->SetPause(false);
			bGamePaused = false;
		}
		
		if (!bMouseCaptured)
		{
			PC->bShowMouseCursor = false;
			FInputModeGameOnly InputMode;
			PC->SetInputMode(InputMode);
			bMouseCaptured = true;
		}
		
		bIsProcessing = false;
	}

	bool AmaterialCharacter::TryPickup()
	{
		if (!FollowCamera || bIsPickingUp) return false;
		const FVector Start = FollowCamera->GetComponentLocation();
		const FVector End   = Start + FollowCamera->GetForwardVector() * PickupRange;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TryPickup), false, this);
		if (!GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity,
			ECC_Camera, FCollisionShape::MakeSphere(PickupSphereRadius), Params)) return false;
		AActor* Target = Hit.GetActor();
		if (!Target) return false;
		const bool bHasValidTag = PickupTags.ContainsByPredicate([Target](const FName& Tag)
			{ return Target->ActorHasTag(Tag); });
		if (!bHasValidTag) return false;
		SetPrimitiveComponentsPhysics(Target, false);
		PendingPickupActor = Target;
		
		float RelativeYaw = Target->GetActorRotation().Yaw - GetActorRotation().Yaw;
		RelativeYaw = FMath::RoundToFloat(RelativeYaw / 90.f) * 90.f;
		HeldRelativeQuat = FQuat(FRotator(0.f, RelativeYaw, 0.f));

		if (PickupAnim && GetMesh())
		{
			GetMesh()->PlayAnimation(PickupAnim, false);
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
		if (!PendingPickupActor || !HoldPivot) return;
		CaptureHeldLocalExtent(PendingPickupActor);

		HeldActor = PendingPickupActor;
		PendingPickupActor = nullptr;
		
		HeldActor->AttachToComponent(HoldPivot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		HeldActor->SetActorRelativeLocation(FVector::ZeroVector);

		FQuat DesiredWorldQuat = GetActorQuat() * HeldRelativeQuat;
		FQuat PivotWorldQuat   = HoldPivot->GetComponentQuat();
		HeldActor->SetActorRelativeRotation((PivotWorldQuat.Inverse() * DesiredWorldQuat).Rotator());

		UpdateHoldPivotTransform();
		ApplyWeightSpeedPenalty(HeldActor);
	}

	void AmaterialCharacter::OnPickupAnimFinished()
	{
		bIsPickingUp = false;
		
		if (HeldActor)
		{
			HeldActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			SetPrimitiveComponentsPhysics(HeldActor, false);
		}
		
		GetWorld()->GetTimerManager().SetTimer(PickupEndTimerHandle, [this]()
		{
			bWasHolding = false;
			bIsPlayingWalk = false;
			UpdateAnimation();
		}, 0.2f, false);
	}

	void AmaterialCharacter::UpdateHeldActorPosition()
	{
		if (!HeldActor) return;

		const float SizeRatio = FMath::Clamp(HeldLocalExtent.GetMax() / 50.f, 0.3f, 1.2f);
		const float DistanceScale = FMath::Lerp(0.6f, 1.05f, SizeRatio);
		const float AdjustedDistance = HoldDistance * DistanceScale;

		const FVector ForwardOffset = GetActorForwardVector() * AdjustedDistance;
		const FVector HeightOffset  = FVector(0.f, 0.f, HoldHeight);
		const FVector TargetLocation = GetActorLocation() + ForwardOffset + HeightOffset;
		
		HeldActor->SetActorLocation(TargetLocation);
		HeldActor->SetActorRotation((GetActorQuat() * HeldRelativeQuat).Rotator());
	}
		
	void AmaterialCharacter::DropHeld()
	{
		if (!HeldActor) return;
		USkeletalMeshComponent* MeshComp = GetMesh();
		if (!PickupAnim || !MeshComp) return;

		if (HoldPivot)
		{
			HeldActor->AttachToComponent(HoldPivot, FAttachmentTransformRules::KeepWorldTransform);
		}

		MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		MeshComp->SetAnimation(PickupAnim);
		MeshComp->SetPlayRate(-1.0f);
		MeshComp->SetPosition(PickupAnim->GetPlayLength());
		MeshComp->Play(false);
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
			HeldActor = nullptr;
		}, DropDetachTime, false);

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
			UpdateAnimation();
		}, PickupAnim->GetPlayLength(), false);
	}

	void AmaterialCharacter::ApplyWeightSpeedPenalty(AActor* Actor)
	{
		if (!Actor) return;
		UCharacterMovementComponent* MoveComp = GetCharacterMovement();
		if (!MoveComp) return;
		OriginalMaxWalkSpeed   = MoveComp->MaxWalkSpeed;
		MoveComp->MaxWalkSpeed = OriginalMaxWalkSpeed * 0.5f;
	}

	void AmaterialCharacter::RestoreWalkSpeed()
	{
		if (OriginalMaxWalkSpeed <= 0.f) return;
		UCharacterMovementComponent* MoveComp = GetCharacterMovement();
		if (MoveComp) MoveComp->MaxWalkSpeed = OriginalMaxWalkSpeed;
		OriginalMaxWalkSpeed = 0.f;
	}

	void AmaterialCharacter::CaptureHeldLocalExtent(AActor* Actor)
	{
		if (!Actor) { HeldLocalExtent = FVector(50.f); return; }
		FVector Origin, BoxExtent;
		Actor->GetActorBounds(false, Origin, BoxExtent);
		HeldLocalExtent = BoxExtent.IsNearlyZero() ? FVector(50.f) : BoxExtent;
	}

	void AmaterialCharacter::UpdateHoldPivotTransform()
	{
		if (!HoldPivot) return;
		const bool bMoving = IsMoving();

		float ObjectScale = 0.4f;

		if (HeldActor)
		{
			if (UStaticMeshComponent* SMC = HeldActor->FindComponentByClass<UStaticMeshComponent>())
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
			ObjectScale
		);

		FVector FinalLoc(-0.20f, AdjustedY, 0.10f);

		FinalLoc += bMoving ? HoldExtraLocalOffset_Walk : HoldExtraLocalOffset_Idle;
		HoldPivot->SetRelativeLocation(FinalLoc);
		HoldPivot->SetRelativeRotation(bMoving ? HoldLocalRot_Walk : HoldLocalRot_Idle);

		if (HeldActor)
		{
			FQuat DesiredWorldQuat = GetActorQuat() * HeldRelativeQuat;
			FQuat PivotWorldQuat   = HoldPivot->GetComponentQuat();
			HeldActor->SetActorRelativeRotation((PivotWorldQuat.Inverse() * DesiredWorldQuat).Rotator());
		}
	}

	void AmaterialCharacter::UpdateAnimation()
	{
		if (bIsPickingUp || !GetMesh()) return;
		const bool bMoving  = IsMoving();
		const bool bHolding = (HeldActor != nullptr);
		if (bWasHolding != bHolding || bMoving != bIsPlayingWalk)
		{
			PlayAnimIfValid(GetAnimForState(bMoving, bHolding), true);
			bWasHolding    = bHolding;
			bIsPlayingWalk = bMoving;
		}
	}

	UAnimSequence* AmaterialCharacter::GetAnimForState(bool bMoving, bool bHolding) const
	{
		if (bMoving) return bHolding ? WalkBringAnim : WalkAnim;
		return bHolding ? IdleBringAnim : IdleAnim;
	}

	void AmaterialCharacter::PlayAnimIfValid(UAnimSequence* Anim, bool bLooping) const
	{
		if (Anim && GetMesh()) GetMesh()->PlayAnimation(Anim, bLooping);
	}

	void AmaterialCharacter::SetPrimitiveComponentsPhysics(AActor* Actor, bool bEnable) const
	{
		if (!Actor) return;
		TArray<UPrimitiveComponent*> PrimComps;
		Actor->GetComponents<UPrimitiveComponent>(PrimComps);
		for (UPrimitiveComponent* PC : PrimComps)
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
			APlayerController* PC = Cast<APlayerController>(GetController());
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
		if (bIsProcessing) return; 
		bIsProcessing = true;
		
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (!PC) 
		{
			bIsProcessing = false;
			return;
		}
		
		bGamePaused = !bGamePaused;
		
		if (bGamePaused)
		{
			PC->SetPause(true);
			PC->bShowMouseCursor = true;
			FInputModeGameAndUI InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(InputMode);
			bMouseCaptured = false;
		}
		else
		{
			PC->SetPause(false);
			PC->bShowMouseCursor = false;
			FInputModeGameOnly InputMode;
			PC->SetInputMode(InputMode);
			bMouseCaptured = true;
		}
		
		bIsProcessing = false;
	}

	void AmaterialCharacter::DecreaseGaugeForMaterial(const FName& MaterialTag)
	{
		UE_LOG(LogTemp, Warning, TEXT("DecreaseGauge: %s"), *MaterialTag.ToString());
		
		if (MaterialTag == TEXT("Joker"))
			return;
			
		if (MaterialTag == TEXT("Rubber"))
			RubberGauge = FMath::Clamp(RubberGauge - GaugeDecreaseAmount, 0, 100);
		else if (MaterialTag == TEXT("Metal"))
			MetalGauge = FMath::Clamp(MetalGauge - GaugeDecreaseAmount, 0, 100);
		else if (MaterialTag == TEXT("Copper"))
			CopperGauge = FMath::Clamp(CopperGauge - GaugeDecreaseAmount, 0, 100);
		else if (MaterialTag == TEXT("Ice"))
			IceGauge = FMath::Clamp(IceGauge - GaugeDecreaseAmount, 0, 100);
		else if (MaterialTag == TEXT("Wood"))
			WoodGauge = FMath::Clamp(WoodGauge - GaugeDecreaseAmount, 0, 100);
		else if (MaterialTag == TEXT("Magnet"))
			MagnetGauge = FMath::Clamp(MagnetGauge - GaugeDecreaseAmount, 0, 100);
	}

	void AmaterialCharacter::OpenRadialMenu(ATransformation_actor* Target)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (!PC) return;

		if (!RadialMenuWidget && RadialMenuClass)
		{
			RadialMenuWidget = CreateWidget<UTestUI>(PC, RadialMenuClass);
			RadialMenuWidget->AddToViewport(100);
			RadialMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
		}

		if (!RadialMenuWidget) return;

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
		if (!RadialMenuWidget) return;

		RadialMenuWidget->SetVisibility(ESlateVisibility::Collapsed);

		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC)
		{
			PC->bShowMouseCursor = false;
			FInputModeGameOnly InputMode;
			PC->SetInputMode(InputMode);
			bMouseCaptured = true;
		}

		if (bConfirm && FollowCamera)
		{
			const FVector Start = FollowCamera->GetComponentLocation();
			const FVector End = Start + FollowCamera->GetForwardVector() * InteractRange;
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(ChangeForm), false, this);

			if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity,
				ECC_Visibility, FCollisionShape::MakeSphere(InteractSphereRadius), Params))
			{
				if (ATransformation_actor* TransformActor = Cast<ATransformation_actor>(Hit.GetActor()))
				{
					RadialMenuWidget->TargetActor = TransformActor;

					if (UseLeftAnim && GetMesh())
					{
						GetMesh()->PlayAnimation(UseLeftAnim, false);
						bIsPickingUp = true;
						bRadialMenuOpen = false;
						GetWorld()->GetTimerManager().SetTimer(RadialMenuAnimTimer, this,
							&AmaterialCharacter::OnUseLeftAnimFinished, UseLeftAnim->GetPlayLength(), false);
						return;
					}
					else
					{
						RadialMenuWidget->ConfirmSelection();
					}
				}
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
		if (RadialMenuWidget)
		{
			RadialMenuWidget->ConfirmSelection();
			RadialMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
		}

		bRadialMenuOpen = false;
		bIsPickingUp = false;
		bWasHolding = false;
		bIsPlayingWalk = false;
		UpdateAnimation();
	}

	void AmaterialCharacter::WarpToLevel(const FString& LevelPath)
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
		WarpToLevel(TEXT("/Game/stage/Stage2/Stage2"));
	}

	void AmaterialCharacter::OnWarpStage3()
	{
		WarpToLevel(TEXT("/Game/stage/Stage3/Stage3"));
	}