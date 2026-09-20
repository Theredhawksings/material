#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Resistance.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UTextRenderComponent;
class AWire;

UCLASS()
class MATERIAL_API AResistance : public AActor
{
    GENERATED_BODY()

public:
    AResistance();

    // Wire.cpp 가 사용하는 인터페이스 (Transformation_actor 와 동일)
    bool  IsConductive()        const { return true; }
    bool  IsElectrified()       const { return bElectrified; }
    float GetBlockResistance()  const { return ResistanceOhm; }
    float GetEffectiveVoltage() const { return StoredVoltage; }
    float GetEffectiveCurrent() const { return StoredCurrent; }

    const TArray<TObjectPtr<AWire>>& GetConnectedWiresList() const { return ConnectedWires; }

    void ReceivePower(float InVoltage, float InCurrent);
    void ClearPower();

    // ── 에디터 설정 ──
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComp;

    // 옴 값 표시 텍스트 (ResistanceOhm 바꾸면 자동 갱신)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UTextRenderComponent> OhmText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance")
    float ResistanceOhm = 1.f;

    // 메시 바운드 반경에 더해지는 여유 감지 거리 (Transformation_actor 와 동일 방식)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance")
    float WireSenseExtraRadius = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance")
    float RefreshInterval = 0.1f;

    // ── 물리 ──
    // 플레이어가 부딪쳐도 잘 안 밀리도록 무겁게 (기본 250kg)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance|Physics")
    float MassKg = 250.f;

    // 밀려도 금방 멈추게 하는 감쇠
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance|Physics")
    float LinearDamping = 4.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance|Physics")
    float AngularDamping = 6.f;

    // 완전 고정. 밀어도 절대 안 움직이고 들어올릴 수도 없음
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance|Physics")
    bool bFixedInPlace = false;

    // 디버그용 표시
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resistance|Debug")
    float DebugVoltage = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resistance|Debug")
    float DebugCurrent = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resistance|Debug")
    bool bDrawDebug = true;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    void RefreshConnectedWires();
    void ApplyPhysicsSettings();

    UPROPERTY(Transient)
    TArray<TObjectPtr<AWire>> ConnectedWires;

    FTimerHandle RefreshTimerHandle;

    bool  bElectrified  = false;
    float StoredVoltage = 0.f;
    float StoredCurrent = 0.f;
};
