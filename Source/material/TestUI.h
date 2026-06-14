#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Transformation_actor.h"
#include "TestUI.generated.h"

UCLASS()
class MATERIAL_API UTestUI : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite)
    ATransformation_actor* TargetActor = nullptr;

    UFUNCTION(BlueprintCallable)
    void ConfirmSelection();

    bool GetSelectedForm(EBlockForm& OutForm) const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    int32 PreviousIndex = -1;

    UFUNCTION(BlueprintImplementableEvent, Category = "Radial Menu")
    void OnSelectedIndexChanged(int32 NewIndex);

    UPROPERTY(meta = (BindWidget))
    class UImage* RadialImage;

private:
    EBlockForm IndexToForm(int32 Index) const;

    UPROPERTY()
    UMaterialInstanceDynamic* DynMaterial = nullptr;
};