// materialAnimInstance.h
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "materialAnimInstance.generated.h"

class UAnimSequence;

// 애니메이션 교체 시 이전 포즈와 크로스페이드해 주는 프록시 (워커 스레드에서 포즈 계산)
struct FMaterialAnimInstanceProxy : public FAnimInstanceProxy
{
	FMaterialAnimInstanceProxy() = default;
	FMaterialAnimInstanceProxy(UAnimInstance* InAnimInstance)
		: FAnimInstanceProxy(InAnimInstance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	// 게임 스레드에서 PreUpdate로 복사되는 상태
	UAnimSequence* CurrentSequence = nullptr;
	UAnimSequence* PreviousSequence = nullptr;
	float CurrentTime = 0.f;
	float PreviousTime = 0.f;
	float BlendAlpha = 1.f;
};

UCLASS()
class MATERIAL_API UmaterialAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// 이전 애니메이션과 BlendTime 동안 크로스페이드하며 재생 (PlayRate < 0 이면 역재생)
	void PlayAnimationSmooth(UAnimSequence* Anim, bool bLooping, float BlendTime = 0.2f,
							 float PlayRate = 1.f, float StartPosition = 0.f);

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

	friend struct FMaterialAnimInstanceProxy;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CurrentSequence;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> PreviousSequence;

	float CurrentTime = 0.f;
	float PreviousTime = 0.f;
	float CurrentRate = 1.f;
	float PreviousRate = 1.f;
	bool bCurrentLooping = false;
	bool bPreviousLooping = false;
	float BlendAlpha = 1.f;
	float BlendDuration = 0.2f;
};
