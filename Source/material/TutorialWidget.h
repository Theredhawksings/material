#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TutorialWidget.generated.h"

UCLASS()
class MATERIAL_API UTutorialWidget : public UUserWidget
{
    GENERATED_BODY()
    
protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
    FName StageLevelName = FName("Stage1-1");
};