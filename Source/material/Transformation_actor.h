#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Transformation_actor.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UPhysicalMaterial;

UENUM(BlueprintType)
enum class EBlockForm : uint8
{
	Ice,
	Rubber,
	Metal,
	Wood
};

USTRUCT(BlueprintType)
struct FBlockFormSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBlockForm Form = EBlockForm::Ice;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<UMaterialInterface*> Materials;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UPhysicalMaterial* PhysMat = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSimulatePhysics = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LinearDamping = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AngularDamping = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideMass = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MassKg = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCanMelt = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCanMagnet = false;
};

UCLASS()
class MATERIAL_API ATransformation_actor : public AActor
{
	GENERATED_BODY()

public:
	ATransformation_actor();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	UStaticMeshComponent* MeshComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Form")
	EBlockForm CurrentForm = EBlockForm::Ice;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Form")
	TArray<FBlockFormSpec> FormSpecs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Form")
	TArray<EBlockForm> CycleOrder = { EBlockForm::Metal, EBlockForm::Ice, EBlockForm::Rubber, EBlockForm::Wood };

	UFUNCTION(BlueprintCallable, Category="Form")
	void SetForm(EBlockForm NewForm);

	UFUNCTION(BlueprintCallable, Category="Form")
	void CycleForm();

	UFUNCTION(BlueprintCallable, Category="Form")
	bool GetCurrentSpec(FBlockFormSpec& OutSpec) const;

private:
	const FBlockFormSpec* FindSpec(EBlockForm Form) const;
	void ApplySpec(const FBlockFormSpec& Spec);
};
