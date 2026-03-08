#include "TutorialWidget.h"
#include "Kismet/GameplayStatics.h"

void UTutorialWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

FReply UTutorialWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        RemoveFromParent();
        
        UGameplayStatics::OpenLevel(GetWorld(), StageLevelName);
        return FReply::Handled();
    }
    
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}