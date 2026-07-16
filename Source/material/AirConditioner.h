#pragma once

#include "CoreMinimal.h"
#include "Temperature.h"
#include "AirConditioner.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UAudioComponent; // ★ 추가
class USoundBase;      // ★ 추가

// ★ 바람 모드 (뜨거운 바람 = 가열 / 차가운 바람 = 냉각)
UENUM(BlueprintType)
enum class EAirconWindMode : uint8
{
    Hot  UMETA(DisplayName = "뜨거운 바람"),
    Cold UMETA(DisplayName = "차가운 바람")
};

UCLASS()
class MATERIAL_API AAirConditioner : public ATemperature
{
    GENERATED_BODY()

public:
    AAirConditioner();

    UFUNCTION(BlueprintCallable, Category = "AirConditioner")
    void ActivateAircon();

    UFUNCTION(BlueprintCallable, Category = "AirConditioner")
    void DeactivateAircon();

    // ★ 바람 모드 전환 (발판에서 호출)
    UFUNCTION(BlueprintCallable, Category = "AirConditioner")
    void SetWindMode(EAirconWindMode NewMode);

    UFUNCTION(BlueprintCallable, Category = "AirConditioner")
    void ToggleWindMode();

    // 히터별 시작 모드 (뜨거운/차가운/뜨거운 은 에디터에서 지정)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner")
    EAirconWindMode WindMode = EAirconWindMode::Hot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AirConditioner")
    bool bIsRunning = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner")
    bool bAlwaysOn = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner")
    float HeatTemperature = 600.0f;

    // ★ 차가운 바람 세기 (가열과 동일한 복사 공식에 쓰이는 크기 값)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner|Cold")
    float ColdWindTemperature = 600.0f;

    // 차가운 바람으로 블럭이 내려갈 수 있는 최저 온도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner|Cold")
    float ColdTargetTemperature = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner|Physics")
    float BlockMassKg = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner|Physics")
    float BlockSpecificHeatJPerKgK = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner|Physics")
    float BlockReceiverAreaM2 = 0.01f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner|Physics")
    float HeatSimTimeScale = 1.0f;

    // ★ 연기(나이아가라) 관련
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner|Smoke")
    TObjectPtr<UNiagaraSystem> SmokeEffect;

    // 에디터에서 켜고 끌 수 있는 연기 분사율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner|Smoke")
    float SmokeSpawnRate = 50.f;

    // ★ 추가: 스모크 사운드
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner|Sound")
    TObjectPtr<USoundBase> SmokeSound;

    // 스모크 작동 후 사운드가 나올 때까지의 딜레이(초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirConditioner|Sound")
    float SmokeSoundDelay = 0.3f;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ★ 연기 노즐 9개 (위치는 에디터에서 조정)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AirConditioner|Smoke")
    TArray<TObjectPtr<UNiagaraComponent>> SmokeComponents;

    void SetSmokeActive(bool bActive);

    // ★ 추가: 사운드 재생 헬퍼 (타이머에서 호출)
    void PlaySmokeSound();

private:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> WireframeMeshComp;

    // ★ 추가: 스모크 사운드용 오디오 컴포넌트
    UPROPERTY(VisibleAnywhere, Category = "AirConditioner|Sound")
    TObjectPtr<UAudioComponent> SmokeAudioComp;

    FTimerHandle SmokeSoundTimer; // ★ 추가: 0.3초 딜레이용

    void HeatNearbyTemperatureBlocks(float DeltaTime);

    // ★ 차가운 바람: 주변 블럭 냉각
    void CoolNearbyTemperatureBlocks(float DeltaTime);

    // ★ 범위 안 가열 대상에게 StopHeating 전송 (끄기/모드 전환 공용)
    void StopHeatingNearbyBlocks();

    static constexpr int32 NumSmokeNozzles = 9;
};