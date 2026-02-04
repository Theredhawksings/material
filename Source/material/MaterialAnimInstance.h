// materialAnimInstance.h
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

protected:
    UPROPERTY(EditAnywhere, Category = "Animation")
    UAnimSequence* WalkAnimation;
};