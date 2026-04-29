#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Syringe.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class AmaterialCharacter;

UENUM(BlueprintType)
enum class ESyringeMaterial : uint8
{
	Metal   UMETA(DisplayName = "Metal"),
	Copper  UMETA(DisplayName = "Copper"),
	Rubber  UMETA(DisplayName = "Rubber"),
	Ice     UMETA(DisplayName = "Ice"),
	Wood    UMETA(DisplayName = "Wood"),
	Magnet  UMETA(DisplayName = "Magnet")
};

USTRUCT(BlueprintType)
struct FSyringeChargeSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESyringeMaterial MaterialType = ESyringeMaterial::Metal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "10"))
	int32 ChargeAmount = 3;
};

UCLASS()
class MATERIAL_API ASyringe : public AActor
{
	GENERATED_BODY()

public:
	ASyringe();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

public:
	UFUNCTION(BlueprintCallable, Category = "Syringe")
	void UseSyringe(AmaterialCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Syringe")
	void AttachToCharacterHand(AmaterialCharacter* Character);

	void StartRotationAnim();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Syringe")
	bool bIsAttached = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Syringe")
	bool bIsUsed = false;

	UPROPERTY(EditAnywhere, Category = "Syringe|Animation")
	FRotator RotAnimEndRot = FRotator(270.f, 0.f, 180.f);

	UPROPERTY(EditAnywhere, Category = "Syringe|Animation")
	float RotAnimDuration = 0.65f;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> OverlapComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Syringe", meta = (AllowPrivateAccess = "true"))
	TArray<FSyringeChargeSpec> ChargeSpecs;

	UPROPERTY(EditAnywhere, Category = "Syringe", meta = (AllowPrivateAccess = "true"))
	float OverlapRadius = 80.f;

	UPROPERTY()
	TObjectPtr<AmaterialCharacter> AttachedCharacter = nullptr;

	FRotator RotAnimStartCached;
	float RotAnimTime = 0.f;
	bool bIsRotating = false;

	static FName MaterialEnumToTag(ESyringeMaterial Material);

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};