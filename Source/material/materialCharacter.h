#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Materials/MaterialParameterCollection.h"
#include "materialCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UAnimSequence;
class UStaticMeshComponent;
class USceneComponent;
class ATransformation_actor;

// ===================== [추가] 열 잔상 슬롯 구조체 =====================
struct FHeatSlot
{
	FVector Position;
	float   Temperature; // 1.0 → 0.0 으로 감소
	float   Radius;      // 초기 반경에서 점점 줄어듦
	bool    bActive;

	FHeatSlot()
		: Position(FVector::ZeroVector)
		, Temperature(0.f)
		, Radius(0.f)
		, bActive(false)
	{}
};
// ====================================================================

UCLASS()
class MATERIAL_API AmaterialCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AmaterialCharacter();
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY() TObjectPtr<UInputMappingContext> IMC_Default;
	UPROPERTY() TObjectPtr<UInputMappingContext> IMC_MouseLook;
	UPROPERTY() TObjectPtr<UInputAction> IA_Move;
	UPROPERTY() TObjectPtr<UInputAction> IA_Look;
	UPROPERTY() TObjectPtr<UInputAction> IA_MouseLook;
	UPROPERTY() TObjectPtr<UInputAction> IA_Jump;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BackpackComp;
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<USceneComponent> HoldPivot;

	UPROPERTY() TObjectPtr<UAnimSequence> WalkAnim;
	UPROPERTY() TObjectPtr<UAnimSequence> IdleAnim;
	UPROPERTY() TObjectPtr<UAnimSequence> PickupAnim;
	UPROPERTY() TObjectPtr<UAnimSequence> IdleBringAnim;
	UPROPERTY() TObjectPtr<UAnimSequence> WalkBringAnim;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TSubclassOf<UAnimInstance> AnimBPClass;

	UPROPERTY() TObjectPtr<AActor> HeldActor;
	UPROPERTY() TObjectPtr<AActor> PendingPickupActor;

	UPROPERTY(EditAnywhere, Category = "Pickup") TArray<FName> PickupTags;
	UPROPERTY(EditAnywhere, Category = "Pickup") FName HoldSocketName = TEXT("hand_RSocket");
	UPROPERTY(EditAnywhere, Category = "Pickup") float PickupRange = 500.f;
	UPROPERTY(EditAnywhere, Category = "Pickup|Offset") FVector HoldExtraLocalOffset_Idle = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category = "Pickup|Offset") FVector HoldExtraLocalOffset_Walk = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category = "Pickup|Offset") FRotator HoldLocalRot_Idle = FRotator(-10.f, -20.f, -10.f);
	UPROPERTY(EditAnywhere, Category = "Pickup|Offset") FRotator HoldLocalRot_Walk = FRotator(-10.f, -20.f, -10.f);

	float OriginalMaxWalkSpeed = 0.f;

	UPROPERTY(EditAnywhere, Category = "Equipment") FName BackpackSocketName = TEXT("spine_002Socket");
	UPROPERTY(EditAnywhere, Category = "Equipment") FVector BackpackRelativeLocation = FVector(0.f, -0.2f, -0.35f);
	UPROPERTY(EditAnywhere, Category = "Equipment") FRotator BackpackRelativeRotation = FRotator(0.f, 0.f, -93.f);
	UPROPERTY(EditAnywhere, Category = "Equipment") FVector BackpackRelativeScale = FVector(0.55f);
	UPROPERTY(EditAnywhere, Category = "Interaction") float InteractRange = 2000.f;
	UPROPERTY(EditAnywhere, Category = "Rendering") int32 CustomDepthStencilValue = 95;
	UPROPERTY(EditAnywhere, Category = "Animation") float WalkSpeedThreshold = 10.f;
	UPROPERTY(EditAnywhere, Category = "Thermal") TObjectPtr<UMaterialParameterCollection> HeatMPC;

	FVector HeldLocalExtent = FVector(50.f);
	FTimerHandle AttachmentTimerHandle;
	FTimerHandle PickupEndTimerHandle;

	uint8 bIsPlayingWalk : 1;
	uint8 bWasHolding    : 1;
	uint8 bIsPickingUp   : 1;


	TArray<FHeatSlot> HeatPool; 
	FTimerHandle      HeatSpawnTimer;

	UPROPERTY(EditAnywhere, Category = "Thermal") float HeatSpawnInterval  = 0.3f; // 잔상 생성 간격(초)
	UPROPERTY(EditAnywhere, Category = "Thermal") float HeatCoolRate       = 1.0f;  // 초당 온도 감소
	UPROPERTY(EditAnywhere, Category = "Thermal") float HeatRadiusDecay    = 60.f;  // 초당 반경 감소
	UPROPERTY(EditAnywhere, Category = "Thermal") float HeatInitialRadius  = 300.f; // 잔상 초기 반경

	void SpawnHeatSlot();                   
	void UpdateHeatSlots(float DeltaTime);  
	// ====================================================================

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void JumpStarted();
	void JumpStopped();
	void ChangeForm();
	void HoldPressed();
	void CheckWeight();
	bool TryPickup();
	void DropHeld();
	void HandleActualAttachment();
	void OnPickupAnimFinished();
	void CaptureHeldLocalExtent(AActor* Actor);
	void ApplyWeightSpeedPenalty(AActor* Actor);
	void RestoreWalkSpeed();
	void UpdateAnimation();
	void UpdateHoldPivotTransform();
	void PlayAnimIfValid(UAnimSequence* Anim, bool bLooping) const;
	UAnimSequence* GetAnimForState(bool bMoving, bool bHolding) const;
	void SetPrimitiveComponentsPhysics(AActor* Actor, bool bEnable) const;

	FORCEINLINE bool IsMoving() const { return GetVelocity().SizeSquared2D() > (WalkSpeedThreshold * WalkSpeedThreshold); }

	static constexpr float CameraArmLength       = 350.f;
	static constexpr float CameraSocketOffsetZ   = 80.f;
	static constexpr float CameraLagSpeed        = 6.0f;
	static constexpr float CameraRotLagSpeed     = 12.0f;
	static constexpr float JumpVelocity          = 600.f;
	static constexpr float AirControl            = 0.2f;
	static constexpr float RotationRate          = 540.f;
	static constexpr float MeshOffsetZ           = -90.f;
	static constexpr float MeshRotationYaw       = 90.f;
	static constexpr float PickupAnimAttachTime  = 1.125f;
	static constexpr float PickupSphereRadius    = 75.f;
	static constexpr float InteractSphereRadius  = 50.f;
	static constexpr float DropForwardOffset     = 3.f;
	static constexpr float DropDetachTime        = 0.83f;
};