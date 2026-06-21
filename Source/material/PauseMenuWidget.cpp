#include "PauseMenuWidget.h"
#include "Components/Button.h"
#include "Kismet/KismetSystemLibrary.h"
#include "materialCharacter.h"

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 위젯이 생성될 때, 각 버튼의 클릭 이벤트에 내 함수들을 연결해 줍니다.
	if (Btn_Resume)
	{
		Btn_Resume->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnResumeClicked);
	}
	if (Btn_Reset)
	{
		Btn_Reset->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnResetClicked);
	}
	if (Btn_Quit)
	{
		Btn_Quit->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnQuitClicked);
	}
}

void UPauseMenuWidget::OnResumeClicked()
{
	// 위젯에서 독단적으로 일시정지를 풀지 말고, 캐릭터에게 지시하여 상태를 동기화합니다.
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (AmaterialCharacter* MyCharacter = Cast<AmaterialCharacter>(PC->GetPawn()))
		{
			// 캐릭터 내부의 함수를 호출하여 위젯 제거 + 일시정지 해제 + 마우스 숨김을 일괄 처리
			MyCharacter->ClosePauseMenu();
		}
	}
}

void UPauseMenuWidget::OnResetClicked()
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (AmaterialCharacter* MyCharacter = Cast<AmaterialCharacter>(PC->GetPawn()))
		{
			MyCharacter->OnResetMap();
		}
	}
}

void UPauseMenuWidget::OnQuitClicked()
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
}