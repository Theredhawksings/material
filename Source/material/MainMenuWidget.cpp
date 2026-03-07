#include "MainMenuWidget.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    if (StartButton)
    {
        StartButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnStartClicked);
    }
    
    if (ExitButton)
    {
        ExitButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnExitClicked);
    }
}

void UMainMenuWidget::OnStartClicked()
{
    RemoveFromParent();
    
    FTimerHandle TimerHandle;
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
    {
        UGameplayStatics::OpenLevel(this, StageLevelName);
    }, 0.1f, false);
}

void UMainMenuWidget::OnExitClicked()
{
    UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, true);
}