#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Syringe.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class AmaterialCharacter;

USTRUCT(BlueprintType)
struct FSyringeChargeSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName MaterialTag = TEXT("Metal");

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

public:
	UFUNCTION(BlueprintCallable, Category = "Syringe")
	void UseSyringe(AmaterialCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Syringe")
	void AttachToCharacterHand(AmaterialCharacter* Character);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Syringe")
	bool bIsAttached = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Syringe")
	bool bIsUsed = false;

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

	static constexpr TCHAR LeftHandSocket[] = TEXT("hand_L_endSocket");

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};