#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

UCLASS()
class MATERIAL_API UMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()
    
protected:
    virtual void NativeConstruct() override;
    
    UPROPERTY(meta = (BindWidget))
    class UButton* StartButton;
    
    UPROPERTY(meta = (BindWidget))
    class UButton* ExitButton;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widgets")
    TSubclassOf<UUserWidget> TutorialWidgetClass;
    
private:
    UFUNCTION()
    void OnStartClicked();
    
    UFUNCTION()
    void OnExitClicked();
};