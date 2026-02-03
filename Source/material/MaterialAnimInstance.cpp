#include "materialAnimInstance.h"
#include "GameFramework/PawnMovementComponent.h"

UmaterialAnimInstance::UmaterialAnimInstance()
{
}

void UmaterialAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    OwnerPawn = TryGetPawnOwner();

    WalkAnimation = LoadObject<UAnimSequence>(nullptr, 
        TEXT("/Game/modeling/Animation/Walk.Walk"));
}

void UmaterialAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!OwnerPawn) return;

    Speed = OwnerPawn->GetVelocity().Size2D();

    if (USkeletalMeshComponent* SkelMesh = GetSkelMeshComponent())
    {
        if (Speed > 10.f && WalkAnimation)
        {
            if (!SkelMesh->IsPlaying())
            {
                SkelMesh->PlayAnimation(WalkAnimation, true);
            }
        }
        else
        {
            if (SkelMesh->IsPlaying())
            {
                SkelMesh->Stop();
            }
        }
    }
}