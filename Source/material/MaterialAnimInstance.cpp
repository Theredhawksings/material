// materialAnimInstance.cpp
#include "materialAnimInstance.h"
#include "Animation/AnimSequence.h"

UmaterialAnimInstance::UmaterialAnimInstance()
{
}

void UmaterialAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    // 올바른 경로 형식 - /Game/경로/파일명.파일명
    WalkAnimation = LoadObject<UAnimSequence>(nullptr, 
        TEXT("/Game/modeling/Animation/Walk.Walk"));

    if (WalkAnimation)
    {
        if (USkeletalMeshComponent* SkelMesh = GetSkelMeshComponent())
        {
            SkelMesh->PlayAnimation(WalkAnimation, true);
        }
    }
}