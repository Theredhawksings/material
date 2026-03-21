// Fill out your copyright notice in the Description page of Project Settings.

#include "TestUI.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Kismet/GameplayStatics.h"

void UTestUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    FVector2D MousePosition;
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);

    // 플레이어의 마우스 위치를 성공적으로 가져왔다면
    if (PC && PC->GetMousePosition(MousePosition.X, MousePosition.Y))
    {
        // 화면 중앙 좌표를 구합니다.
        FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(GetWorld());
        FVector2D CenterPosition = ViewportSize / 2.0f;

        // 중앙에서 마우스까지 방향과 거리를 계산합니다.
        FVector2D Direction = MousePosition - CenterPosition;
        float Distance = Direction.Size();

        float Deadzone = 50.0f; // 50 픽셀 반경 안에서는 입력을 무시합니다.
        int32 Segments = 8;     // 8방향 메뉴

        if (Distance > Deadzone)
        {
            // 각도 계산 (Atan2 활용)
            float Angle = FMath::Atan2(Direction.Y, Direction.X) * (180.0f / PI);

            // 음수인 경우 360도 체계로 변환
            if (Angle < 0.0f)
            {
                Angle += 360.0f;
            }

            // 섹터의 위치를 맞추기 위해 각도 틀어주기
            float OffsetAngle = Angle + (360.0f / Segments / 2.0f);
            if (OffsetAngle >= 360.0f)
            {
                OffsetAngle -= 360.0f;
            }

            // 선택 인덱스 계산
            int32 SelectedIndex = FMath::FloorToInt(OffsetAngle / (360.0f / Segments));

            // 선택이 바뀌었을 때만 블루프린트 이벤트 호출
            if (SelectedIndex != PreviousIndex)
            {
                OnSelectedIndexChanged(SelectedIndex);
                PreviousIndex = SelectedIndex;
            }
        }
        else
        {
            // 마우스가 데드존 안에 있으면 선택 없음(-1) 신호 전송
            if (PreviousIndex != -1)
            {
                OnSelectedIndexChanged(-1);
                PreviousIndex = -1;
            }
        }
    }
}