// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TestUI.generated.h"

/**
*
 */
UCLASS()
class MATERIAL_API UTestUI : public UUserWidget
{
    GENERATED_BODY()

protected:
    // 매 프레임 마우스 위치를 추적하기 위해 틱 함수를 씁니다.
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // 이전 프레임에 선택됐던 인덱스를 기억하는 변수입니다.
    int32 PreviousIndex = -1;

    // 인덱스가 바뀔 때마다 블루프린트로 신호를 보내는 이벤트입니다.
    UFUNCTION(BlueprintImplementableEvent, Category = "Radial Menu")
    void OnSelectedIndexChanged(int32 NewIndex);
};