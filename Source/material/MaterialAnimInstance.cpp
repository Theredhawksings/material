// materialAnimInstance.cpp
#include "materialAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"

namespace
{
	float AdvanceAnimTime(float Time, float DeltaSeconds, float Rate, float Length, bool bLooping)
	{
		if (Length <= 0.f)
			return 0.f;

		Time += DeltaSeconds * Rate;
		if (bLooping)
		{
			Time = FMath::Fmod(Time, Length);
			if (Time < 0.f)
				Time += Length;
			return Time;
		}
		return FMath::Clamp(Time, 0.f, Length);
	}
}

void UmaterialAnimInstance::PlayAnimationSmooth(UAnimSequence* Anim, bool bLooping, float BlendTime, float PlayRate, float StartPosition)
{
	if (!Anim)
		return;

	// 재생 중이던 애니메이션을 블렌드 아웃 대상으로 보관
	if (CurrentSequence && BlendTime > 0.f)
	{
		PreviousSequence = CurrentSequence;
		PreviousTime     = CurrentTime;
		PreviousRate     = CurrentRate;
		bPreviousLooping = bCurrentLooping;
		BlendAlpha       = 0.f;
		BlendDuration    = BlendTime;
	}
	else
	{
		PreviousSequence = nullptr;
		BlendAlpha       = 1.f;
	}

	CurrentSequence = Anim;
	CurrentTime     = FMath::Clamp(StartPosition, 0.f, Anim->GetPlayLength());
	CurrentRate     = PlayRate;
	bCurrentLooping = bLooping;
}

void UmaterialAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (CurrentSequence)
		CurrentTime = AdvanceAnimTime(CurrentTime, DeltaSeconds, CurrentRate, CurrentSequence->GetPlayLength(), bCurrentLooping);

	if (PreviousSequence)
	{
		PreviousTime = AdvanceAnimTime(PreviousTime, DeltaSeconds, PreviousRate, PreviousSequence->GetPlayLength(), bPreviousLooping);

		BlendAlpha += (BlendDuration > 0.f) ? (DeltaSeconds / BlendDuration) : 1.f;
		if (BlendAlpha >= 1.f)
		{
			BlendAlpha = 1.f;
			PreviousSequence = nullptr;
		}
	}
}

FAnimInstanceProxy* UmaterialAnimInstance::CreateAnimInstanceProxy()
{
	return new FMaterialAnimInstanceProxy(this);
}

void FMaterialAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	if (const UmaterialAnimInstance* Inst = Cast<UmaterialAnimInstance>(InAnimInstance))
	{
		CurrentSequence  = Inst->CurrentSequence;
		PreviousSequence = Inst->PreviousSequence;
		CurrentTime      = Inst->CurrentTime;
		PreviousTime     = Inst->PreviousTime;
		BlendAlpha       = Inst->BlendAlpha;
	}
}

bool FMaterialAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	if (!CurrentSequence)
	{
		Output.ResetToRefPose();
		return true;
	}

	const FAnimExtractContext CurrentContext(static_cast<double>(CurrentTime), false);

	// 블렌드가 필요 없으면 현재 애니메이션 포즈만 추출
	if (!PreviousSequence || BlendAlpha >= 1.f)
	{
		FAnimationPoseData OutPoseData(Output);
		CurrentSequence->GetAnimationPose(OutPoseData, CurrentContext);
		return true;
	}

	// 이전/현재 포즈를 각각 추출한 뒤 알파로 섞기
	FPoseContext CurrentPose(this);
	FPoseContext PreviousPose(this);
	FAnimationPoseData CurrentPoseData(CurrentPose);
	FAnimationPoseData PreviousPoseData(PreviousPose);

	CurrentSequence->GetAnimationPose(CurrentPoseData, CurrentContext);
	PreviousSequence->GetAnimationPose(PreviousPoseData, FAnimExtractContext(static_cast<double>(PreviousTime), false));

	FAnimationPoseData OutPoseData(Output);
	const float Weight = FMath::SmoothStep(0.f, 1.f, BlendAlpha);
	FAnimationRuntime::BlendTwoPosesTogether(CurrentPoseData, PreviousPoseData, Weight, OutPoseData);
	return true;
}
