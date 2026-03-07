#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

UCLASS()
class MATERIAL_API UMainMenuWidget : public UUserWidget  // YOURGAME_API → MATERIAL_API
{
    GENERATED_BODY()
    
protected:
    virtual void NativeConstruct() override;
    
    UPROPERTY(meta = (BindWidget))
    class UButton* StartButton;
    
    UPROPERTY(meta = (BindWidget))
    class UButton* ExitButton;
    
    // FName으로 변경 (FString에서)
    UPROPERTY(EditAnywhere, Category = "Level")
    FName StageLevelName = FName("Stage1-1");
    
private:
    UFUNCTION()
    void OnStartClicked();
    
    UFUNCTION()
    void OnExitClicked();
};