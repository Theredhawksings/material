// EndingElevator.h
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Elevator.h"
#include "Blueprint/UserWidget.h"
#include "materialCharacter.h"
#include "EndingElevator.generated.h"

UCLASS()
class MATERIAL_API AEndingElevator : public AElevator
{
	GENERATED_BODY()

public:
	AEndingElevator();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ─── 엔딩 설정 (에디터에서 조절) ───

	UPROPERTY(EditAnywhere, Category = "Ending")
	float EndingStartDelay = 10.f;

	UPROPERTY(EditAnywhere, Category = "Ending")
	float FadeDuration = 3.f;

	// 암전 시작 후 텍스트 나오기까지 딜레이
	UPROPERTY(EditAnywhere, Category = "Ending")
	float TextDelay = 2.f;

	// 암전 + 텍스트 다 본 후 메뉴 이동까지 여유
	UPROPERTY(EditAnywhere, Category = "Ending")
	float AfterFadeDelay = 2.f;

	UPROPERTY(EditAnywhere, Category = "Ending")
	TSubclassOf<UUserWidget> EndingWidgetClass;

	UPROPERTY(EditAnywhere, Category = "Ending")
	FName MainMenuLevel = FName("/Game/stage/MainMenu");

	// ─── 내부 함수 ───
	UFUNCTION()
	void OnEndingTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	void StartEnding();
	void ShowEndingText();

	// ─── 내부 상태 ───
	UPROPERTY()
	TObjectPtr<UUserWidget> EndingWidget = nullptr;

	bool bEndingStarted = false;
	bool bPlayerBoarded = false;
	float BoardElapsed = 0.f;
};