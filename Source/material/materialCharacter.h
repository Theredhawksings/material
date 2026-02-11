#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "materialCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UAnimSequence;
class UStaticMeshComponent;
class USceneComponent;

UCLASS()
class MATERIAL_API AmaterialCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AmaterialCharacter();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera, meta=(AllowPrivateAccess="true"))
    USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera, meta=(AllowPrivateAccess="true"))
    UCameraComponent* FollowCamera;

    UPROPERTY() UInputMappingContext* IMC_Default;
    UPROPERTY() UInputMappingContext* IMC_MouseLook;
    UPROPERTY() UInputAction* IA_Move;
    UPROPERTY() UInputAction* IA_Look;
    UPROPERTY() UInputAction* IA_MouseLook;
    UPROPERTY() UInputAction* IA_Jump;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment", meta=(AllowPrivateAccess="true"))
    UStaticMeshComponent* BackpackComp;

    UPROPERTY(VisibleAnywhere, Category="Pickup")
    USceneComponent* HoldPivot;

    UPROPERTY() UAnimSequence* WalkAnim;
    UPROPERTY() UAnimSequence* IdleAnim;
    UPROPERTY() UAnimSequence* PickupAnim;
    UPROPERTY() UAnimSequence* IdleBringAnim;
    UPROPERTY() UAnimSequence* WalkBringAnim;

    UPROPERTY(EditAnywhere, Category="Animation")
    TSubclassOf<UAnimInstance> AnimBPClass;

    UPROPERTY() AActor* HeldActor;
    UPROPERTY() AActor* PendingPickupActor;
    
    UPROPERTY(EditAnywhere, Category="Pickup")
    TArray<FName> PickupTags;

    UPROPERTY(EditAnywhere, Category="Pickup")
    FName HoldSocketName = TEXT("hand_RSocket");

    UPROPERTY(EditAnywhere, Category="Pickup")
    float PickupRange = 500.f;

    UPROPERTY()
    FVector HeldLocalExtent = FVector(50.f);

    UPROPERTY(EditAnywhere, Category="Equipment")
    FName BackpackSocketName = TEXT("spine_002Socket");

    UPROPERTY(EditAnywhere, Category="Equipment")
    FVector BackpackRelativeLocation = FVector(0.f, -0.2f, -0.35f);

    UPROPERTY(EditAnywhere, Category="Equipment")
    FRotator BackpackRelativeRotation = FRotator(0.f, 0.f, -93.f);

    UPROPERTY(EditAnywhere, Category="Equipment")
    FVector BackpackRelativeScale = FVector(0.55f);

    UPROPERTY(EditAnywhere, Category="Interaction")
    float InteractRange = 2000.f;

    UPROPERTY(EditAnywhere, Category="Rendering")
    int32 CustomDepthStencilValue = 125;

    uint8 bIsPlayingWalk:1;
    uint8 bWasHolding:1;
    uint8 bIsPickingUp:1;

    FTimerHandle AttachmentTimerHandle;
    FTimerHandle PickupEndTimerHandle;

    UPROPERTY(EditAnywhere, Category="Animation")
    float WalkSpeedThreshold = 10.f;

    UPROPERTY(EditAnywhere, Category="Pickup|Offset")
    FVector HoldExtraLocalOffset_Idle = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category="Pickup|Offset")
    FVector HoldExtraLocalOffset_Walk = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category="Pickup|Offset")
    FRotator HoldLocalRot_Idle = FRotator(-10.f, -20.f, -10.f);

    UPROPERTY(EditAnywhere, Category="Pickup|Offset")
    FRotator HoldLocalRot_Walk = FRotator(-10.f, -20.f, -10.f);

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void JumpStarted();
    void JumpStopped();
    void ChangeForm();
    void HoldPressed();

    bool TryPickup();
    void DropHeld();
    void HandleActualAttachment();
    void OnPickupAnimFinished();
    void CaptureHeldLocalExtent(AActor* Actor);

    void UpdateAnimation();
    void UpdateHoldPivotTransform();
    
    FORCEINLINE bool IsMoving() const { return GetVelocity().SizeSquared2D() > (WalkSpeedThreshold * WalkSpeedThreshold); }

public:
    FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
    FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};