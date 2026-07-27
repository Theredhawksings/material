// ElevatorCameraShake.cpp
#include "ElevatorCameraShake.h"
#include "Shakes/PerlinNoiseCameraShakePattern.h"

UElevatorCameraShake::UElevatorCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UPerlinNoiseCameraShakePattern* Pattern =
		ObjectInitializer.CreateDefaultSubobject<UPerlinNoiseCameraShakePattern>(this, TEXT("PerlinPattern"));

	Pattern->Duration     = 0.7f;
	Pattern->BlendInTime  = 0.05f;
	Pattern->BlendOutTime = 0.25f;

	// 위치 흔들림 (도착 시 덜컹거림 - Z 위주)
	Pattern->X.Amplitude = 2.5f;
	Pattern->X.Frequency = 22.f;
	Pattern->Y.Amplitude = 2.0f;
	Pattern->Y.Frequency = 18.f;
	Pattern->Z.Amplitude = 4.0f;
	Pattern->Z.Frequency = 28.f;

	// 회전 흔들림 (살짝만)
	Pattern->Pitch.Amplitude = 0.6f;
	Pattern->Pitch.Frequency = 22.f;
	Pattern->Roll.Amplitude  = 0.4f;
	Pattern->Roll.Frequency  = 18.f;

	SetRootShakePattern(Pattern);
}
