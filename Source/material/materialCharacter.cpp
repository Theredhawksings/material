#include "materialCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Components/InputComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputActionValue.h"

#include "Animation/AnimSequence.h"
#include "UObject/ConstructorHelpers.h"

#include "Transformation_actor.h"
#include "Engine/World.h"
#include "TimerManager.h"

AmaterialCharacter::AmaterialCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
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

    HoldPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HoldPivot"));
    HoldPivot->SetupAttachment(GetMesh());

    static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAsset(TEXT("AnimSequence'/Game/modeling/Animation/Walk.Walk'"));
    if (WalkAsset.Succeeded()) WalkAnim = WalkAsset.Object;

    static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleAsset(TEXT("AnimSequence'/Game/modeling/Animation/Test.Test'"));
    if (IdleAsset.Succeeded()) IdleAnim = IdleAsset.Object;

    static ConstructorHelpers::FObjectFinder<UAnimSequence> PickupAsset(TEXT("AnimSequence'/Game/modeling/Animation/bring1.bring1'"));
    if (PickupAsset.Succeeded()) PickupAnim = PickupAsset.Object;

    static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleBringAsset(TEXT("AnimSequence'/Game/modeling/Animation/idle_bring.idle_bring'"));
    if (IdleBringAsset.Succeeded()) IdleBringAnim = IdleBringAsset.Object;

    static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkBringAsset(TEXT("AnimSequence'/Game/modeling/Animation/Walk_bring.Walk_bring'"));
    if (WalkBringAsset.Succeeded()) WalkBringAnim = WalkBringAsset.Object;

    static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC_DefaultAsset(TEXT("InputMappingContext'/Game/Input/IMC_Default.IMC_Default'"));
    if (IMC_DefaultAsset.Succeeded()) IMC_Default = IMC_DefaultAsset.Object;

    static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC_MouseLookAsset(TEXT("InputMappingContext'/Game/Input/IMC_MouseLook.IMC_MouseLook'"));
    if (IMC_MouseLookAsset.Succeeded()) IMC_MouseLook = IMC_MouseLookAsset.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> IA_MoveAsset(TEXT("InputAction'/Game/Input/Actions/IA_Move.IA_Move'"));
    if (IA_MoveAsset.Succeeded()) IA_Move = IA_MoveAsset.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> IA_LookAsset(TEXT("InputAction'/Game/Input/Actions/IA_Look.IA_Look'"));
    if (IA_LookAsset.Succeeded()) IA_Look = IA_LookAsset.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> IA_MouseLookAsset(TEXT("InputAction'/Game/Input/Actions/IA_MouseLook.IA_MouseLook'"));
    if (IA_MouseLookAsset.Succeeded()) IA_MouseLook = IA_MouseLookAsset.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> IA_JumpAsset(TEXT("InputAction'/Game/Input/Actions/IA_Jump.IA_Jump'"));
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

    static ConstructorHelpers::FObjectFinder<UStaticMesh> BackpackMeshAsset(
        TEXT("StaticMesh'/Game/modeling/Character/backPack/BackPack_final.BackPack_final'")
    );
    if (BackpackMeshAsset.Succeeded()) BackpackComp->SetStaticMesh(BackpackMeshAsset.Object);
}

void AmaterialCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                if (IMC_Default) Subsystem->AddMappingContext(IMC_Default, 0);
                if (IMC_MouseLook) Subsystem->AddMappingContext(IMC_MouseLook, 1);
            }
        }
    }

    if (GetMesh())
    {
        GetMesh()->SetRenderCustomDepth(true);
        GetMesh()->SetCustomDepthStencilValue(CustomDepthStencilValue);
        GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        if (IdleAnim) GetMesh()->PlayAnimation(IdleAnim, true);
    }

    if (HoldPivot && GetMesh() && GetMesh()->DoesSocketExist(HoldSocketName))
    {
        HoldPivot->AttachToComponent(
            GetMesh(),
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            HoldSocketName
        );
        HoldPivot->SetRelativeLocation(FVector::ZeroVector);
        HoldPivot->SetRelativeRotation(FRotator::ZeroRotator);
    }

    if (BackpackComp && GetMesh())
    {
        BackpackComp->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, BackpackSocketName);
        BackpackComp->SetRelativeLocation(BackpackRelativeLocation);
        BackpackComp->SetRelativeRotation(BackpackRelativeRotation);
        BackpackComp->SetRelativeScale3D(BackpackRelativeScale);
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

    FBox Box(ForceInit);
    for (UPrimitiveComponent* PC : PrimComps)
    {
        if (PC) Box += PC->CalcBounds(FTransform::Identity).GetBox();
    }

    HeldLocalExtent = Box.IsValid ? Box.GetExtent() : FVector(50.f);
}

void AmaterialCharacter::UpdateHoldPivotTransform()
{
    if (!HoldPivot || !GetMesh()) return;

    float LeftShift = -0.55f;
    float ForwardShift = -0.4f;
    float UpShift = 0.1f;

    FVector FinalLoc = FVector(LeftShift, ForwardShift, UpShift);

    if (GetVelocity().Size2D() > WalkSpeedThreshold)
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
    // ✅ bIsPickingUp이 true인 동안은 Idle/Walk 애니메이션이 재생되지 않음 (bring1 보호)
    if (!GetMesh() || bIsPickingUp) return;

    const float Speed = GetVelocity().Size2D();
    const bool bHolding = (HeldActor != nullptr);

    if (bWasHolding != bHolding)
    {
        if (Speed > WalkSpeedThreshold)
        {
            if (bHolding && WalkBringAnim) GetMesh()->PlayAnimation(WalkBringAnim, true);
            else if (WalkAnim)             GetMesh()->PlayAnimation(WalkAnim, true);
        }
        else
        {
            if (bHolding && IdleBringAnim) GetMesh()->PlayAnimation(IdleBringAnim, true);
            else if (IdleAnim)             GetMesh()->PlayAnimation(IdleAnim, true);
        }

        bWasHolding = bHolding;
        bIsPlayingWalk = (Speed > WalkSpeedThreshold);
        return;
    }

    if (Speed > WalkSpeedThreshold)
    {
        if (!bIsPlayingWalk)
        {
            if (bHolding && WalkBringAnim) GetMesh()->PlayAnimation(WalkBringAnim, true);
            else if (WalkAnim)             GetMesh()->PlayAnimation(WalkAnim, true);
            bIsPlayingWalk = true;
        }
    }
    else
    {
        if (bIsPlayingWalk)
        {
            if (bHolding && IdleBringAnim) GetMesh()->PlayAnimation(IdleBringAnim, true);
            else if (IdleAnim)             GetMesh()->PlayAnimation(IdleAnim, true);
            bIsPlayingWalk = false;
        }
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

void AmaterialCharacter::JumpStarted() { Jump(); }
void AmaterialCharacter::JumpStopped() { StopJumping(); }

void AmaterialCharacter::ChangeForm()
{
    if (!FollowCamera) return;

    const FVector Start = FollowCamera->GetComponentLocation();
    const FVector End   = Start + FollowCamera->GetForwardVector() * InteractRange;

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

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
    if (HeldActor) { DropHeld(); return; }
    TryPickup();
}

bool AmaterialCharacter::TryPickup()
{
    if (!FollowCamera || bIsPickingUp) return false;

    const FVector Start = FollowCamera->GetComponentLocation();
    const FVector End   = Start + FollowCamera->GetForwardVector() * PickupRange;

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Camera, Params)) return false;

    AActor* Target = Hit.GetActor();
    if (!Target) return false;

    bool bAllowed = false;
    for (const FName& Tag : PickupTags)
    {
        if (Target->ActorHasTag(Tag)) { bAllowed = true; break; }
    }
    if (!bAllowed) return false;

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

    if (PickupAnim && GetMesh())
    {
        // 1. 애니메이션 재생 (루프X)
        GetMesh()->PlayAnimation(PickupAnim, false);
        bIsPickingUp = true;

        // 2. 타이머 A: 1.125초 시점에 물건을 손에 부착 (애니메이션은 계속 재생됨)
        GetWorld()->GetTimerManager().SetTimer(
            AttachmentTimerHandle, this,
            &AmaterialCharacter::HandleActualAttachment,
            1.125f, false
        );

        // 3. 타이머 B: 애니메이션 실제 종료 시점에 캐릭터 상태 해제
        float AnimDuration = PickupAnim->GetPlayLength();
        GetWorld()->GetTimerManager().SetTimer(
            PickupEndTimerHandle, this,
            &AmaterialCharacter::OnPickupAnimFinished,
            AnimDuration, false
        );
    }

    return true;
}

// ✅ 1.125초 시점에 실행될 함수 (물리적 부착만 담당)
void AmaterialCharacter::HandleActualAttachment()
{
    if (!PendingPickupActor || !HoldPivot) return;

    CaptureHeldLocalExtent(PendingPickupActor);

    HeldActor = PendingPickupActor;
    PendingPickupActor = nullptr;

    HeldActor->AttachToComponent(
        HoldPivot,
        FAttachmentTransformRules::SnapToTargetNotIncludingScale
    );
    HeldActor->SetActorRelativeLocation(FVector::ZeroVector);
    HeldActor->SetActorRelativeRotation(FRotator::ZeroRotator);

    UpdateHoldPivotTransform();
}

// ✅ 애니메이션이 완전히 끝난 시점에 실행될 함수 (상태 해제만 담당)
void AmaterialCharacter::OnPickupAnimFinished()
{
    bIsPickingUp = false;
    bWasHolding = false;
    bIsPlayingWalk = false;
    
    // 즉시 애니메이션 상태 갱신
    UpdateAnimation();
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

    if (IdleAnim && GetMesh()) GetMesh()->PlayAnimation(IdleAnim, true);

    if (HoldPivot)
    {
        HoldPivot->SetRelativeLocation(FVector::ZeroVector);
        HoldPivot->SetRelativeRotation(FRotator::ZeroRotator);
    }
}