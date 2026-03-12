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
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Transformation_actor.h"
#include "TimerManager.h"
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

	HoldPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HoldPivot"));
	HoldPivot->SetupAttachment(GetMesh());

	BackpackComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackpackComp"));
	BackpackComp->SetupAttachment(GetMesh());
	BackpackComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BackpackMeshAsset(
		TEXT("StaticMesh'/Game/modeling/Character/backPack/BackPack_final.BackPack_final'"));
	if (BackpackMeshAsset.Succeeded())
		BackpackComp->SetStaticMesh(BackpackMeshAsset.Object);

	struct FAnimLoader { const TCHAR* Path; TObjectPtr<UAnimSequence>* Target; };
	const FAnimLoader AnimAssets[] = {
		{ TEXT("AnimSequence'/Game/modeling/Animation/Walk1.Walk1'"),             &WalkAnim },
		{ TEXT("AnimSequence'/Game/modeling/Animation/Test.Test'"),               &IdleAnim },
		{ TEXT("AnimSequence'/Game/modeling/Animation/bring2.bring2'"),           &PickupAnim },
		{ TEXT("AnimSequence'/Game/modeling/Animation/idle_bring2.idle_bring2'"), &IdleBringAnim },
		{ TEXT("AnimSequence'/Game/modeling/Animation/Walk_bring1.Walk_bring1'"), &WalkBringAnim },
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
	};
	for (const FActionLoader& Loader : InputActions)
	{
		ConstructorHelpers::FObjectFinder<UInputAction> ActionAsset(Loader.Path);
		if (ActionAsset.Succeeded()) *Loader.Target = ActionAsset.Object;
	}

	if (PickupTags.Num() == 0)
		PickupTags = { TEXT("Metal"), TEXT("Rubber"), TEXT("Ice"), TEXT("Wood") };

	static ConstructorHelpers::FObjectFinder<UMaterial> PlayerMat(
		TEXT("/Script/Engine.Material'/Game/modeling/Character/M_Character.M_Character'"));
	if (PlayerMat.Succeeded()) GetMesh()->SetMaterial(0, PlayerMat.Object);

	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> HeatMPCAsset(
		TEXT("/Script/Engine.MaterialParameterCollection'/Game/MPC_HeatSources.MPC_HeatSources'"));
	if (HeatMPCAsset.Succeeded()) HeatMPC = HeatMPCAsset.Object;
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

	MeshComp->SetRenderCustomDepth(true);
	MeshComp->SetCustomDepthStencilValue(CustomDepthStencilValue);
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

	HeatPool.SetNum(7);
	for (FHeatSlot& Slot : HeatPool)
		Slot.bActive = false;

	GetWorld()->GetTimerManager().SetTimer(
		HeatSpawnTimer, this,
		&AmaterialCharacter::SpawnHeatSlot,
		HeatSpawnInterval, true);
}

void AmaterialCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HeldActor) UpdateHoldPivotTransform();
	UpdateAnimation();

	if (HeatMPC)
	{
		UMaterialParameterCollectionInstance* MPCInst = GetWorld()->GetParameterCollectionInstance(HeatMPC);
		if (MPCInst)
		{
			if (IsMoving() && !bIsPickingUp)
				HeatPos1CurrentRadius = FMath::Clamp(
					HeatPos1CurrentRadius + HeatPos1GrowRate * DeltaTime, 0.f, 300.f);
			else
				HeatPos1CurrentRadius = FMath::Clamp(
					HeatPos1CurrentRadius - HeatPos1ShrinkRate * DeltaTime, 0.f, 300.f);

			FVector Pos = GetActorLocation() + FVector(0.f, 0.f, -90.f);
			MPCInst->SetVectorParameterValue(FName("HeatPos1"),
				FLinearColor(Pos.X, Pos.Y, Pos.Z, HeatPos1CurrentRadius));
		}
	}

	UpdateHeatSlots(DeltaTime);
}

void AmaterialCharacter::SpawnHeatSlot()
{
	if (!IsMoving() || bIsPickingUp) return;

	for (FHeatSlot& Slot : HeatPool)
	{
		if (!Slot.bActive)
		{
			Slot.Position    = GetActorLocation() + FVector(0.f, 0.f, -90.f);
			Slot.Temperature = 1.0f;
			Slot.Radius      = HeatInitialRadius;
			Slot.bActive     = true;
			return;
		}
	}

	int32 OldestIdx = 0;
	float MinTemp   = HeatPool[0].Temperature;
	for (int32 i = 1; i < HeatPool.Num(); i++)
	{
		if (HeatPool[i].Temperature < MinTemp)
		{
			MinTemp   = HeatPool[i].Temperature;
			OldestIdx = i;
		}
	}
	HeatPool[OldestIdx].Position    = GetActorLocation() + FVector(0.f, 0.f, -90.f);
	HeatPool[OldestIdx].Temperature = 1.0f;
	HeatPool[OldestIdx].Radius      = HeatInitialRadius;
	HeatPool[OldestIdx].bActive     = true;
}

void AmaterialCharacter::UpdateHeatSlots(float DeltaTime)
{
	if (!HeatMPC) return;
	UMaterialParameterCollectionInstance* MPCInst = GetWorld()->GetParameterCollectionInstance(HeatMPC);
	if (!MPCInst) return;

	TSet<AActor*> HeatedActors;

	for (int32 i = 0; i < HeatPool.Num(); i++)
	{
		FHeatSlot& Slot     = HeatPool[i];
		FName      ParamName = FName(*FString::Printf(TEXT("HeatPos%d"), i + 2));

		if (!Slot.bActive)
		{
			MPCInst->SetVectorParameterValue(ParamName,
				FLinearColor(-999999.f, -999999.f, -999999.f, 0.f));
			continue;
		}

		Slot.Temperature -= HeatCoolRate    * DeltaTime;
		Slot.Radius      -= HeatRadiusDecay * DeltaTime;

		if (Slot.Temperature < 0.05f || Slot.Radius < 10.f)
		{
			Slot.bActive = false;
			MPCInst->SetVectorParameterValue(ParamName,
				FLinearColor(-999999.f, -999999.f, -999999.f, 0.f));
			continue;
		}

		MPCInst->SetVectorParameterValue(ParamName,
			FLinearColor(Slot.Position.X, Slot.Position.Y, Slot.Position.Z,
				Slot.Radius * Slot.Temperature));
		float CollisionRadius = Slot.Radius * FMath::Pow(Slot.Temperature, 3.0f);
		DrawDebugSphere(GetWorld(), Slot.Position, CollisionRadius, 16, FColor::Red, false, 0.0f);

		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HeatOverlap), false, this);
		GetWorld()->OverlapMultiByChannel(
			Overlaps, Slot.Position, FQuat::Identity,
			ECC_Visibility, FCollisionShape::MakeSphere(CollisionRadius), Params);

		for (FOverlapResult& Overlap : Overlaps)
		{
			AActor* HitActor = Overlap.GetActor();
			if (!HitActor || HitActor == this) continue;

			const bool bHasTag = PickupTags.ContainsByPredicate([HitActor](const FName& Tag)
				{ return HitActor->ActorHasTag(Tag); });
			if (!bHasTag) continue;

			HeatedActors.Add(HitActor);
			float& Heat = ActorHeatMap.FindOrAdd(HitActor, 0.f);
			Heat = FMath::Clamp(Heat + ActorHeatIncreaseRate * DeltaTime, 0.f, 1.f);
		}
	}

	for (auto It = ActorHeatMap.CreateIterator(); It; ++It)
	{
		AActor* Actor = It.Key();
		float&  Heat  = It.Value();

		if (!HeatedActors.Contains(Actor))
			Heat = FMath::Clamp(Heat - ActorHeatDecayRate * DeltaTime, 0.f, 1.f);

		TArray<UPrimitiveComponent*> PrimComps;
		Actor->GetComponents<UPrimitiveComponent>(PrimComps);
		int32 StencilVal = FMath::RoundToInt(
			FMath::Lerp((float)ColdStencilValue, (float)HotStencilValue, Heat));

		for (UPrimitiveComponent* PC : PrimComps)
			if (PC) PC->SetCustomDepthStencilValue(StencilVal);

		if (Heat <= 0.f) It.RemoveCurrent();
	}
}

void AmaterialCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction("ChangeForm",  IE_Pressed, this, &AmaterialCharacter::ChangeForm);
	PlayerInputComponent->BindAction("Hold",        IE_Pressed, this, &AmaterialCharacter::HoldPressed);
	PlayerInputComponent->BindAction("Checkweight", IE_Pressed, this, &AmaterialCharacter::CheckWeight);

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
	if (!FollowCamera) return;
	const FVector Start = FollowCamera->GetComponentLocation();
	const FVector End   = Start + FollowCamera->GetForwardVector() * InteractRange;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ChangeForm), false, this);
	if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity,
		ECC_Visibility, FCollisionShape::MakeSphere(InteractSphereRadius), Params))
	{
		if (ATransformation_actor* TransformActor = Cast<ATransformation_actor>(Hit.GetActor()))
			TransformActor->NextForm();
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
	if (HeldActor) DropHeld();
	else TryPickup();
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
	HeldActor          = PendingPickupActor;
	PendingPickupActor = nullptr;
	HeldActor->AttachToComponent(HoldPivot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	HeldActor->SetActorRelativeLocation(FVector::ZeroVector);
	HeldActor->SetActorRelativeRotation(FRotator(0.f, 8.f, 0.f));
	UpdateHoldPivotTransform();
	ApplyWeightSpeedPenalty(HeldActor);
}

void AmaterialCharacter::OnPickupAnimFinished()
{
	bIsPickingUp = false;
	GetWorld()->GetTimerManager().SetTimer(PickupEndTimerHandle, [this]()
	{
		bWasHolding    = false;
		bIsPlayingWalk = false;
		UpdateAnimation();
	}, 0.2f, false);
}

void AmaterialCharacter::DropHeld()
{
	if (!HeldActor) return;
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!PickupAnim || !MeshComp) return;
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
		HeldActor->SetActorLocation(HeldActor->GetActorLocation() + GetActorForwardVector() * DropForwardOffset);
		SetPrimitiveComponentsPhysics(HeldActor, true);
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
	TArray<UPrimitiveComponent*> PrimComps;
	Actor->GetComponents<UPrimitiveComponent>(PrimComps);
	if (PrimComps.Num() == 0) { HeldLocalExtent = FVector(50.f); return; }
	FBox Box(ForceInit);
	for (const UPrimitiveComponent* PC : PrimComps)
		if (PC) Box += PC->CalcBounds(FTransform::Identity).GetBox();
	HeldLocalExtent = Box.IsValid ? Box.GetExtent() : FVector(50.f);
}

void AmaterialCharacter::UpdateHoldPivotTransform()
{
	if (!HoldPivot) return;
	const bool bMoving = IsMoving();
	FVector FinalLoc(-0.35f, -0.4f, 0.1f);
	FinalLoc += bMoving ? HoldExtraLocalOffset_Walk : HoldExtraLocalOffset_Idle;
	HoldPivot->SetRelativeLocation(FinalLoc);
	HoldPivot->SetRelativeRotation(bMoving ? HoldLocalRot_Walk : HoldLocalRot_Idle);
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
			PC->SetSimulatePhysics(bEnable);
			PC->SetEnableGravity(bEnable);
			PC->SetCollisionEnabled(bEnable ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		}
	}
}