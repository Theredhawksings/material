#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "materialCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class MATERIAL_API AmaterialCharacter : public ACharacter
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
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

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void JumpStarted();
	void JumpStopped();

public:
	AmaterialCharacter();
};
