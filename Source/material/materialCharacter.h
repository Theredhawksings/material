// materialCharacter.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "materialCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UAnimSequence;
struct FInputActionValue;

UCLASS()
class MATERIAL_API AmaterialCharacter : public ACharacter
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
    USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
    UCameraComponent* FollowCamera;

    UPROPERTY()
    UInputMappingContext* IMC_Default;

    UPROPERTY()
    UInputMappingContext* IMC_MouseLook;

    UPROPERTY()
    UInputAction* IA_Move;

    UPROPERTY()
    UInputAction* IA_Look;

    UPROPERTY()
    UInputAction* IA_MouseLook;

    UPROPERTY()
    UInputAction* IA_Jump;

    UPROPERTY(EditAnywhere, Category = "Interaction")
    float InteractRange = 2000.f;
    
    UPROPERTY(EditAnywhere, Category="Pickup")
    TArray<FName> PickupTags;

    UPROPERTY(EditAnywhere, Category="Pickup")
    FName HoldSocketName = TEXT("hand_RSocket");

    UPROPERTY(EditAnywhere, Category="Pickup")
    float PickupRange = 500.f;

    UPROPERTY()
    AActor* HeldActor = nullptr;

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

    UPROPERTY(EditAnywhere, Category="Animation")
    UAnimSequence* WalkAnim = nullptr;

    UPROPERTY(EditAnywhere, Category="Animation")
    UAnimSequence* WalkBringAnim = nullptr;

    UPROPERTY(EditAnywhere, Category="Animation")
    float WalkPlayRate = 1.0f;

    bool bIsPlayingWalk = false;

public:
    AmaterialCharacter();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void JumpStarted();
    void JumpStopped();
    void ChangeForm();

    void HoldPressed();
    bool TryPickup();
    void DropHeld();

    void UpdateAnimation();

public:
    FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
    FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};