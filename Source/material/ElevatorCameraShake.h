// ElevatorCameraShake.h
#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "ElevatorCameraShake.generated.h"

// 엘리베이터 도착 직전에 재생되는 카메라 흔들림
UCLASS()
class MATERIAL_API UElevatorCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UElevatorCameraShake(const FObjectInitializer& ObjectInitializer);
};
