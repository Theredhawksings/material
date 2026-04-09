#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "TestUI.h"
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

struct FHeatSlot
{
	FVector Position;
	float   Temperature;
	float   Radius;
	bool    bActive;

	FHeatSlot()
		: Position(FVector::ZeroVector)
		, Temperature(0.f)
		, Radius(0.f)
		, bActive(false)
	{}
};

UCLASS()
class MATERIAL_API AmaterialCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AmaterialCharacter();
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	
	void OnLeftClick();
	void OnEscapePressed();
	void DecreaseGaugeForMaterial(const FName& MaterialTag);
	
	UFUNCTION(BlueprintCallable, Category = "Material Gauge")
	float GetRubberGauge() const { return RubberGauge; }
	
	UFUNCTION(BlueprintCallable, Category = "Material Gauge")
	float GetMetalGauge() const { return MetalGauge; }
	
	UFUNCTION(BlueprintCallable, Category = "Material Gauge")
	float GetIceGauge() const { return IceGauge; }
	
	UFUNCTION(BlueprintCallable, Category = "Material Gauge")
	float GetWoodGauge() const { return WoodGauge; }
	
	UFUNCTION(BlueprintCallable, Category = "Material Gauge")
	float GetMagnetGauge() const { return MagnetGauge; }

	UFUNCTION(BlueprintCallable, Category = "Material Gauge")
	float GetRubberGaugePercent() const { return RubberGauge / 100.f; }
	
	UFUNCTION(BlueprintCallable, Category = "Material Gauge")
	float GetMetalGaugePercent() const { return MetalGauge / 100.f; }
	
	UFUNCTION(BlueprintCallable, Category = "Material Gauge")
	float GetIceGaugePercent() const { return IceGauge / 100.f; }
	
	UFUNCTION(BlueprintCallable, Category = "Material Gauge")
	float GetWoodGaugePercent() const { return WoodGauge / 100.f; }
	
	UFUNCTION(BlueprintCallable, Category = "Material Gauge")
	float GetMagnetGaugePercent() const { return MagnetGauge / 100.f; }

	UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UTestUI> RadialMenuClass;

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
	UPROPERTY() TObjectPtr<UInputAction> IA_LeftClick;
	UPROPERTY() TObjectPtr<UInputAction> IA_Escape; 

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BackpackComp;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ArmComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ArmComp2;

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<USceneComponent> HoldPivot;

	UPROPERTY() TObjectPtr<UAnimSequence> WalkAnim;
	UPROPERTY() TObjectPtr<UAnimSequence> IdleAnim;
	UPROPERTY() TObjectPtr<UAnimSequence> PickupAnim;
	UPROPERTY() TObjectPtr<UAnimSequence> IdleBringAnim;
	UPROPERTY() TObjectPtr<UAnimSequence> WalkBringAnim;
	UPROPERTY() TObjectPtr<UAnimSequence> UseEAnim;
	UPROPERTY() TObjectPtr<UAnimSequence> UseLeftAnim;	

	UPROPERTY(EditAnywhere, Category = "Animation")
	TSubclassOf<UAnimInstance> AnimBPClass;

	UPROPERTY() TObjectPtr<AActor> HeldActor;
	UPROPERTY() TObjectPtr<AActor> PendingPickupActor;

	UPROPERTY(EditAnywhere, Category = "Pickup") TArray<FName> PickupTags;
	UPROPERTY(EditAnywhere, Category = "Pickup") FName HoldSocketName = TEXT("hand_RSocket");
	UPROPERTY(EditAnywhere, Category = "Equipment") FName ArmSocketName2 = TEXT("hand_LSocket");
	UPROPERTY(EditAnywhere, Category = "Pickup") float PickupRange = 500.f;
	UPROPERTY(EditAnywhere, Category = "Pickup") float HoldDistance = 85.f; 
	UPROPERTY(EditAnywhere, Category = "Pickup") float HoldHeight = 65.f;   
	UPROPERTY(EditAnywhere, Category = "Pickup|Offset") FVector HoldExtraLocalOffset_Idle = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category = "Pickup|Offset") FVector HoldExtraLocalOffset_Walk = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category = "Pickup|Offset") FRotator HoldLocalRot_Idle = FRotator(-10.f, -20.f, -10.f);
	UPROPERTY(EditAnywhere, Category = "Pickup|Offset") FRotator HoldLocalRot_Walk = FRotator(-10.f, -20.f, -10.f);

	float OriginalMaxWalkSpeed = 0.f;
	FVector HeldLocalExtent = FVector(50.f);
	FQuat HeldRelativeQuat = FQuat::Identity;
	
	UPROPERTY(EditAnywhere, Category = "Equipment") FName BackpackSocketName = TEXT("spine_002Socket");
	UPROPERTY(EditAnywhere, Category = "Equipment") FVector BackpackRelativeLocation = FVector(0.f, -0.2f, -0.35f);
	UPROPERTY(EditAnywhere, Category = "Equipment") FRotator BackpackRelativeRotation = FRotator(0.f, 0.f, -93.f);
	UPROPERTY(EditAnywhere, Category = "Equipment") FVector BackpackRelativeScale = FVector(0.55f);
	UPROPERTY(EditAnywhere, Category = "Interaction") float InteractRange = 2000.f;
	UPROPERTY(EditAnywhere, Category = "Rendering") int32 CustomDepthStencilValue = 0;
	UPROPERTY(EditAnywhere, Category = "Animation") float WalkSpeedThreshold = 10.f;
	UPROPERTY(EditAnywhere, Category = "Thermal") TObjectPtr<UMaterialParameterCollection> HeatMPC;
	
	UPROPERTY(BlueprintReadOnly, Category = "Material Gauge", meta = (AllowPrivateAccess = "true"))
	float RubberGauge = 100.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Material Gauge", meta = (AllowPrivateAccess = "true"))
	float MetalGauge = 100.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Material Gauge", meta = (AllowPrivateAccess = "true"))
	float IceGauge = 100.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Material Gauge", meta = (AllowPrivateAccess = "true"))
	float WoodGauge = 100.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Material Gauge", meta = (AllowPrivateAccess = "true"))
	float MagnetGauge = 100.f;

	UPROPERTY(EditAnywhere, Category = "Material Gauge")
	float GaugeDecreaseAmount = 10.f;

	FTimerHandle AttachmentTimerHandle;
	FTimerHandle PickupEndTimerHandle;

	uint8 bIsPlayingWalk : 1;
	uint8 bWasHolding    : 1;
	uint8 bIsPickingUp   : 1;

	TArray<FHeatSlot> HeatPool;
	FTimerHandle      HeatSpawnTimer;

	UPROPERTY(EditAnywhere, Category = "Thermal") float HeatSpawnInterval    = 0.3f;
	UPROPERTY(EditAnywhere, Category = "Thermal") float HeatInitialRadius    = 300.f;
	UPROPERTY(EditAnywhere, Category = "Thermal") float HeatPos1ShrinkRate  = 80.f;
	UPROPERTY(EditAnywhere, Category = "Thermal") float HeatPos1GrowRate    = 400.f;
	UPROPERTY(EditAnywhere, Category = "Thermal") float HeatCoolRate        = 0.3f;
	UPROPERTY(EditAnywhere, Category = "Thermal") float HeatRadiusDecay     = 20.f;
	UPROPERTY(EditAnywhere, Category = "Thermal") float ActorHeatIncreaseRate = 2.0f;
	UPROPERTY(EditAnywhere, Category = "Thermal") float ActorHeatDecayRate    = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Thermal") int32 ColdStencilValue      = 1;
	UPROPERTY(EditAnywhere, Category = "Thermal") int32 HotStencilValue        = 5;

	float HeatPos1CurrentRadius = 300.f;

	TMap<AActor*, float> ActorHeatMap;
	TMap<AActor*, int32> ActorBaseStencilMap;

	bool bMouseCaptured = false;
	bool bHadFocusBefore = false;
	bool bGamePaused = false;

	void SpawnHeatSlot();
	void UpdateHeatSlots(float DeltaTime);

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void JumpStarted();
	void JumpStopped();
	void ChangeForm();
	void HoldPressed();
	void CheckWeight();

	void OnWindowFocusChanged(bool bHasFocus);

	bool TryPickup();
	void DropHeld();
	void HandleActualAttachment();
	void OnPickupAnimFinished();
	void UpdateHeldActorPosition();
	void UpdateHoldPivotTransform();
	void CaptureHeldLocalExtent(AActor* Actor);
	void ApplyWeightSpeedPenalty(AActor* Actor);
	void RestoreWalkSpeed();
	void UpdateAnimation();
	void PlayAnimIfValid(UAnimSequence* Anim, bool bLooping) const;
	UAnimSequence* GetAnimForState(bool bMoving, bool bHolding) const;
	void SetPrimitiveComponentsPhysics(AActor* Actor, bool bEnable) const;

	FORCEINLINE bool IsMoving() const { return GetVelocity().SizeSquared2D() > (WalkSpeedThreshold * WalkSpeedThreshold); }

	static constexpr float CameraArmLength      = 350.f;
	static constexpr float CameraSocketOffsetZ  = 80.f;
	static constexpr float CameraLagSpeed       = 6.0f;
	static constexpr float CameraRotLagSpeed    = 12.0f;
	static constexpr float JumpVelocity         = 600.f;
	static constexpr float AirControl           = 0.2f;
	static constexpr float RotationRate         = 540.f;
	static constexpr float MeshOffsetZ          = -90.f;
	static constexpr float MeshRotationYaw      = 90.f;
	static constexpr float PickupAnimAttachTime = 1.125f;
	static constexpr float PickupSphereRadius   = 75.f;
	static constexpr float InteractSphereRadius = 50.f;
	static constexpr float DropForwardOffset    = 15.f;
	static constexpr float DropDetachTime       = 1.03f;

	UPROPERTY()
    UTestUI* RadialMenuWidget = nullptr;

    bool bRadialMenuOpen = false;

    void OpenRadialMenu(ATransformation_actor* Target);
    void CloseRadialMenu(bool bConfirm);

	UPROPERTY() ATransformation_actor* PendingRadialTarget = nullptr;
	FTimerHandle RadialMenuAnimTimer;
	void OnUseEAnimFinished();
	void OnUseLeftAnimFinished();

	FTimerHandle MagnetSettleTimerHandle;

	void WarpToLevel(const FString& LevelPath);
	void OnWarpLaboratory();
	void OnWarpStage1();
	void OnWarpStage2();
	void OnWarpStage3();
	
};