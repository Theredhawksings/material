#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

// UButton 클래스를 쓰겠다고 미리 알려줌 (컴파일 속도 향상)
class UButton;

UCLASS()
class MATERIAL_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// 위젯이 화면에 생성될 때 자동으로 한 번 실행되는 함수
	virtual void NativeConstruct() override;

	// BindWidget은 블루프린트의 버튼 이름과 이 변수명을 강제로 연결합니다.
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Resume;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Reset;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Quit;

private:
	// 버튼을 클릭했을 때 실행될 함수들 (반드시 UFUNCTION()을 붙여야 UI 이벤트와 연결됨)
	UFUNCTION()
	void OnResumeClicked();

	UFUNCTION()
	void OnResetClicked();

	UFUNCTION()
	void OnQuitClicked();
};