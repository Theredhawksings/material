#include "materialCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Animation/AnimSequence.h"
#include "UObject/ConstructorHelpers.h"
#include "Transformation_actor.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace CharacterConstants
{
    constexpr float CameraArmLength = 350.f;
    constexpr float CameraSocketOffsetZ = 80.f;
    constexpr float CameraLagSpeed = 6.0f;
    constexpr float CameraRotLagSpeed = 12.0f;
    constexpr float JumpVelocity = 600.f;
    constexpr float AirControl = 0.2f;
    constexpr float RotationRate = 540.f;
    constexpr float MeshOffsetZ = -90.f;
    constexpr float MeshRotationYaw = 90.f;
    constexpr float PickupAnimAttachTime = 1.125f;
}

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
    Movement->RotationRate = FRotator(0.f, CharacterConstants::RotationRate, 0.f);
    Movement->JumpZVelocity = CharacterConstants::JumpVelocity;
    Movement->AirControl = CharacterConstants::AirControl;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = CharacterConstants::CameraArmLength;
    CameraBoom->SocketOffset = FVector(0.f, 0.f, CharacterConstants::CameraSocketOffsetZ);
    CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = CharacterConstants::CameraLagSpeed;
    CameraBoom->bEnableCameraRotationLag = true;
    CameraBoom->CameraRotationLagSpeed = CharacterConstants::CameraRotLagSpeed;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom);
    FollowCamera->bUsePawnControlRotation = false;

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(TEXT("SkeletalMesh'/Game/modeling/Character/Astronier.Astronier'"));
    if (MeshAsset.Succeeded())
    {
        USkeletalMeshComponent* MeshComp = GetMesh();
        MeshComp->SetSkeletalMesh(MeshAsset.Object);
        MeshComp->SetRelativeLocation(FVector(0.f, 0.f, CharacterConstants::MeshOffsetZ));
        MeshComp->SetRelativeRotation(FRotator(0.f, CharacterConstants::MeshRotationYaw, 0.f));
    }

    static ConstructorHelpers::FClassFinder<UAnimInstance> AnimBPAsset(TEXT("AnimBlueprint'/Game/modeling/Character/Astronier_Skeleton_AnimBlueprint.Astronier_Skeleton_AnimBlueprint_C'"));
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

    static ConstructorHelpers::FObjectFinder<UStaticMesh> BackpackMeshAsset(TEXT("StaticMesh'/Game/modeling/Character/backPack/BackPack_final.BackPack_final'"));
    if (BackpackMeshAsset.Succeeded())
    {
        BackpackComp->SetStaticMesh(BackpackMeshAsset.Object);
    }

    struct FAnimAssetLoader
    {
        const TCHAR* Path;
        UAnimSequence** Target;
    };

    FAnimAssetLoader AnimAssets[] = {
        { TEXT("AnimSequence'/Game/modeling/Animation/Walk1.Walk1'"), &WalkAnim },
        { TEXT("AnimSequence'/Game/modeling/Animation/Test.Test'"), &IdleAnim },
        { TEXT("AnimSequence'/Game/modeling/Animation/bring2.bring2'"), &PickupAnim },
        { TEXT("AnimSequence'/Game/modeling/Animation/idle_bring1.idle_bring1'"), &IdleBringAnim },
        { TEXT("AnimSequence'/Game/modeling/Animation/Walk_bring1.Walk_bring1'"), &WalkBringAnim }
    };

    for (const FAnimAssetLoader& Loader : AnimAssets)
    {
        ConstructorHelpers::FObjectFinder<UAnimSequence> AnimAsset(Loader.Path);
        if (AnimAsset.Succeeded())
        {
            *Loader.Target = AnimAsset.Object;
        }
    }

    static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC_DefaultAsset(TEXT("InputMappingContext'/Game/Input/IMC_Default.IMC_Default'"));
    if (IMC_DefaultAsset.Succeeded()) IMC_Default = IMC_DefaultAsset.Object;

    static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC_MouseLookAsset(TEXT("InputMappingContext'/Game/Input/IMC_MouseLook.IMC_MouseLook'"));
    if (IMC_MouseLookAsset.Succeeded()) IMC_MouseLook = IMC_MouseLookAsset.Object;

    struct FInputActionLoader
    {
        const TCHAR* Path;
        UInputAction** Target;
    };

    FInputActionLoader InputActions[] = {
        { TEXT("InputAction'/Game/Input/Actions/IA_Move.IA_Move'"), &IA_Move },
        { TEXT("InputAction'/Game/Input/Actions/IA_Look.IA_Look'"), &IA_Look },
        { TEXT("InputAction'/Game/Input/Actions/IA_MouseLook.IA_MouseLook'"), &IA_MouseLook },
        { TEXT("InputAction'/Game/Input/Actions/IA_Jump.IA_Jump'"), &IA_Jump }
    };

    for (const FInputActionLoader& Loader : InputActions)
    {
        ConstructorHelpers::FObjectFinder<UInputAction> ActionAsset(Loader.Path);
        if (ActionAsset.Succeeded())
        {
            *Loader.Target = ActionAsset.Object;
        }
    }

    if (PickupTags.Num() == 0)
    {
        PickupTags.Reserve(4);
        PickupTags.Add(TEXT("Metal"));
        PickupTags.Add(TEXT("Rubber"));
        PickupTags.Add(TEXT("Ice"));
        PickupTags.Add(TEXT("Wood"));
    }
}

void AmaterialCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            if (IMC_Default) Subsystem->AddMappingContext(IMC_Default, 0);
            if (IMC_MouseLook) Subsystem->AddMappingContext(IMC_MouseLook, 1);
        }
    }

    USkeletalMeshComponent* MeshComp = GetMesh();
    if (MeshComp)
    {
        MeshComp->SetRenderCustomDepth(true);
        MeshComp->SetCustomDepthStencilValue(CustomDepthStencilValue);

        MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        if (IdleAnim)
        {
            MeshComp->PlayAnimation(IdleAnim, true);
        }

        if (HoldPivot && MeshComp->DoesSocketExist(HoldSocketName))
        {
            HoldPivot->AttachToComponent(MeshComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HoldSocketName);
            HoldPivot->SetRelativeLocation(FVector::ZeroVector);
            HoldPivot->SetRelativeRotation(FRotator::ZeroRotator);
        }

        if (BackpackComp)
        {
            BackpackComp->AttachToComponent(MeshComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, BackpackSocketName);
            BackpackComp->SetRelativeLocation(BackpackRelativeLocation);
            BackpackComp->SetRelativeRotation(BackpackRelativeRotation);
            BackpackComp->SetRelativeScale3D(BackpackRelativeScale);
        }
    }
}

void AmaterialCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (HeldActor)
    {
        UpdateHoldPivotTransform();
    }

    UpdateAnimation();
}

void AmaterialCharacter::CaptureHeldLocalExtent(AActor* Actor)
{
    if (!Actor) return;

    TArray<UPrimitiveComponent*> PrimComps;
    Actor->GetComponents<UPrimitiveComponent>(PrimComps);

    if (PrimComps.Num() == 0)
    {
        HeldLocalExtent = FVector(50.f);
        return;
    }

    FBox Box(ForceInit);
    for (UPrimitiveComponent* PC : PrimComps)
    {
        if (PC)
        {
            Box += PC->CalcBounds(FTransform::Identity).GetBox();
        }
    }

    HeldLocalExtent = Box.IsValid ? Box.GetExtent() : FVector(50.f);
}

void AmaterialCharacter::UpdateHoldPivotTransform()
{
    if (!HoldPivot) return;

    constexpr float LeftShift = -0.35f;
    constexpr float ForwardShift = -0.4f;
    constexpr float UpShift = 0.1f;

    FVector FinalLoc(LeftShift, ForwardShift, UpShift);

    if (IsMoving())
    {
        FinalLoc += HoldExtraLocalOffset_Walk;
        HoldPivot->SetRelativeRotation(HoldLocalRot_Walk);
    }
    else
    {
        FinalLoc += HoldExtraLocalOffset_Idle;
        HoldPivot->SetRelativeRotation(HoldLocalRot_Idle);
    }

    HoldPivot->SetRelativeLocation(FinalLoc);
}

void AmaterialCharacter::UpdateAnimation()
{
    if (bIsPickingUp) return;

    USkeletalMeshComponent* MeshComp = GetMesh();
    if (!MeshComp) return;

    const bool bMoving = IsMoving();
    const bool bHolding = (HeldActor != nullptr);

    if (bWasHolding != bHolding)
    {
        UAnimSequence* TargetAnim = nullptr;
        
        if (bMoving)
        {
            TargetAnim = bHolding ? WalkBringAnim : WalkAnim;
        }
        else
        {
            TargetAnim = bHolding ? IdleBringAnim : IdleAnim;
        }

        if (TargetAnim)
        {
            MeshComp->PlayAnimation(TargetAnim, true);
        }

        bWasHolding = bHolding;
        bIsPlayingWalk = bMoving;
        return;
    }

    if (bMoving != bIsPlayingWalk)
    {
        UAnimSequence* TargetAnim = nullptr;

        if (bMoving)
        {
            TargetAnim = bHolding ? WalkBringAnim : WalkAnim;
        }
        else
        {
            TargetAnim = bHolding ? IdleBringAnim : IdleAnim;
        }

        if (TargetAnim)
        {
            MeshComp->PlayAnimation(TargetAnim, true);
        }

        bIsPlayingWalk = bMoving;
    }
}

void AmaterialCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAction("ChangeForm", IE_Pressed, this, &AmaterialCharacter::ChangeForm);
    PlayerInputComponent->BindAction("Hold", IE_Pressed, this, &AmaterialCharacter::HoldPressed);

    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!EIC) return;

    if (IA_Move) EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AmaterialCharacter::Move);
    if (IA_Look) EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AmaterialCharacter::Look);
    if (IA_MouseLook) EIC->BindAction(IA_MouseLook, ETriggerEvent::Triggered, this, &AmaterialCharacter::Look);

    if (IA_Jump)
    {
        EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &AmaterialCharacter::JumpStarted);
        EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AmaterialCharacter::JumpStopped);
    }
}

void AmaterialCharacter::Move(const FInputActionValue& Value)
{
    if (!Controller) return;

    const FVector2D Axis = Value.Get<FVector2D>();
    if (Axis.IsNearlyZero()) return;

    const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
    const FRotationMatrix RotMatrix(YawRot);

    AddMovementInput(RotMatrix.GetUnitAxis(EAxis::X), Axis.Y);
    AddMovementInput(RotMatrix.GetUnitAxis(EAxis::Y), Axis.X);
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
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ChangeForm), false, this);

    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
    {
        if (ATransformation_actor* TransformActor = Cast<ATransformation_actor>(Hit.GetActor()))
        {
            TransformActor->NextForm();
        }
    }
}

void AmaterialCharacter::HoldPressed()
{
    if (HeldActor)
    {
        DropHeld();
    }
    else
    {
        TryPickup();
    }
}

bool AmaterialCharacter::TryPickup()
{
    if (!FollowCamera || bIsPickingUp) return false;

    const FVector Start = FollowCamera->GetComponentLocation();
    const FVector End = Start + FollowCamera->GetForwardVector() * PickupRange;

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(TryPickup), false, this);

    if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Camera, Params))
    {
        return false;
    }

    AActor* Target = Hit.GetActor();
    if (!Target) return false;

    bool bHasValidTag = false;
    for (const FName& Tag : PickupTags)
    {
        if (Target->ActorHasTag(Tag))
        {
            bHasValidTag = true;
            break;
        }
    }

    if (!bHasValidTag) return false;

    TArray<UPrimitiveComponent*> PrimComps;
    Target->GetComponents<UPrimitiveComponent>(PrimComps);
    for (UPrimitiveComponent* PC : PrimComps)
    {
        if (PC)
        {
            PC->SetSimulatePhysics(false);
            PC->SetEnableGravity(false);
            PC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }

    PendingPickupActor = Target;

    USkeletalMeshComponent* MeshComp = GetMesh();
    if (PickupAnim && MeshComp)
    {
        MeshComp->PlayAnimation(PickupAnim, false);
        bIsPickingUp = true;

        GetWorld()->GetTimerManager().SetTimer(
            AttachmentTimerHandle,
            this,
            &AmaterialCharacter::HandleActualAttachment,
            CharacterConstants::PickupAnimAttachTime,
            false
        );

        const float AnimDuration = PickupAnim->GetPlayLength();
        GetWorld()->GetTimerManager().SetTimer(
            PickupEndTimerHandle,
            this,
            &AmaterialCharacter::OnPickupAnimFinished,
            AnimDuration,
            false
        );
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
    HeldActor->SetActorRelativeRotation(FRotator(0.f, 8.f, 0.f));

    UpdateHoldPivotTransform();
}

void AmaterialCharacter::OnPickupAnimFinished()
{
    bIsPickingUp = false;

    if (HeldActor)
    {
        //HeldActor->SetActorRelativeLocation(FVector::ZeroVector);
        //HeldActor->SetActorRelativeRotation(FRotator(0.f, 23.f, 0.f));
    }

    GetWorld()->GetTimerManager().SetTimer(
        PickupEndTimerHandle,
        [this]()
        {
            bWasHolding = false;
            bIsPlayingWalk = false;
            UpdateAnimation();
        },
        0.2f,
        false
    );
}

void AmaterialCharacter::DropHeld()
{
    if (!HeldActor) return;

    HeldActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    TArray<UPrimitiveComponent*> PrimComps;
    HeldActor->GetComponents<UPrimitiveComponent>(PrimComps);
    for (UPrimitiveComponent* PC : PrimComps)
    {
        if (PC)
        {
            PC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            PC->SetEnableGravity(true);
            PC->SetSimulatePhysics(true);
        }
    }

    HeldActor = nullptr;
    bWasHolding = false;
    bIsPlayingWalk = false;

    USkeletalMeshComponent* MeshComp = GetMesh();
    if (IdleAnim && MeshComp)
    {
        MeshComp->PlayAnimation(IdleAnim, true);
    }

    if (HoldPivot)
    {
        HoldPivot->SetRelativeLocation(FVector::ZeroVector);
        HoldPivot->SetRelativeRotation(FRotator::ZeroRotator);
    }
}