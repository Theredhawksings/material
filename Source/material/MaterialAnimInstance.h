#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "materialAnimInstance.generated.h"

UCLASS()
class MATERIAL_API UmaterialAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UmaterialAnimInstance();

    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
    UPROPERTY(EditAnywhere, Category = "Animation")
    UAnimSequence* WalkAnimation = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Animation")
    float Speed = 0.f;

    UPROPERTY()
    APawn* OwnerPawn = nullptr;
};