#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "materialCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UAnimSequence;
class UStaticMeshComponent;
class USceneComponent;
struct FInputActionValue;

UCLASS()
class MATERIAL_API AmaterialCharacter : public ACharacter
{
    GENERATED_BODY()

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

    UPROPERTY(EditAnywhere, Category="Interaction")
    float InteractRange = 2000.f;

    UPROPERTY(EditAnywhere, Category="Pickup")
    TArray<FName> PickupTags;

    UPROPERTY(EditAnywhere, Category="Pickup")
    FName HoldSocketName = TEXT("hand_RSocket");

    UPROPERTY(EditAnywhere, Category="Pickup")
    float PickupRange = 500.f;

    UPROPERTY() AActor* HeldActor = nullptr;
    UPROPERTY() AActor* PendingPickupActor = nullptr;

    // ✅ 소켓에 붙어서 “오프셋만” 담당할 피벗
    UPROPERTY(VisibleAnywhere, Category="Pickup")
    USceneComponent* HoldPivot = nullptr;

    UPROPERTY()
    FVector HeldLocalExtent = FVector(50.f, 50.f, 50.f);

    UPROPERTY(EditAnywhere, Category="Rendering")
    int32 CustomDepthStencilValue = 125;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment", meta=(AllowPrivateAccess="true"))
    UStaticMeshComponent* BackpackComp;

    UPROPERTY(EditAnywhere, Category="Equipment")
    FName BackpackSocketName = TEXT("spine_002Socket");

    UPROPERTY(EditAnywhere, Category="Equipment")
    FVector BackpackRelativeLocation = FVector(0.f, -0.2f, -0.35f);

    UPROPERTY(EditAnywhere, Category="Equipment")
    FRotator BackpackRelativeRotation = FRotator(0.f, 0.f, -93.f);

    UPROPERTY(EditAnywhere, Category="Equipment")
    FVector BackpackRelativeScale = FVector(0.55f, 0.55f, 0.55f);

    UPROPERTY() UAnimSequence* WalkAnim = nullptr;
    UPROPERTY() UAnimSequence* IdleAnim = nullptr;
    UPROPERTY() UAnimSequence* PickupAnim = nullptr;
    UPROPERTY() UAnimSequence* IdleBringAnim = nullptr;
    UPROPERTY() UAnimSequence* WalkBringAnim = nullptr;

    bool bIsPlayingWalk = false;
    bool bWasHolding = false;
    bool bIsPickingUp = false;

    FTimerHandle PickupTimerHandle;

    UPROPERTY(EditAnywhere, Category="Animation")
    float WalkSpeedThreshold = 10.f;

    // ✅ 오프셋 멀티(Extent 기반)
    UPROPERTY(EditAnywhere, Category="Pickup|Offset")
    float HoldMulForward_Idle = 0.01f;

    UPROPERTY(EditAnywhere, Category="Pickup|Offset")
    float HoldMulRight_Idle = 0.006f;

    UPROPERTY(EditAnywhere, Category="Pickup|Offset")
    float HoldMulForward_Walk = 0.01f;

    UPROPERTY(EditAnywhere, Category="Pickup|Offset")
    float HoldMulRight_Walk = 0.006f;

    // ✅ 추가 고정 오프셋(필요하면 여기서 미세조정)
    UPROPERTY(EditAnywhere, Category="Pickup|Offset")
    float HoldExtraUp = 0.f;

    UPROPERTY(EditAnywhere, Category="Pickup|Offset")
    FVector HoldExtraLocalOffset_Idle = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category="Pickup|Offset")
    FVector HoldExtraLocalOffset_Walk = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category="Pickup|Offset")
    FRotator HoldLocalRot_Idle = FRotator(-10.f, -20.f, -10.f);

    UPROPERTY(EditAnywhere, Category="Pickup|Offset")
    FRotator HoldLocalRot_Walk = FRotator(-10.f, -20.f, -10.f);

public:
    AmaterialCharacter();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void JumpStarted();
    void JumpStopped();
    void ChangeForm();

    void HoldPressed();
    bool TryPickup();
    void DropHeld();
    void OnPickupAnimFinished();

    void UpdateAnimation();
    void CaptureHeldLocalExtent(AActor* Actor);

    void UpdateHoldPivotTransform();

    void HandleActualAttachment();

    FTimerHandle AttachmentTimerHandle;
    FTimerHandle PickupEndTimerHandle;

public:
    FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
    FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};
