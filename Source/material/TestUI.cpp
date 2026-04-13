#include "TestUI.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

void UTestUI::NativeConstruct()
{
    Super::NativeConstruct();

    if (RadialImage)
    {
        UMaterialInterface* BaseMat = Cast<UMaterialInterface>(
            RadialImage->GetBrush().GetResourceObject());

        if (BaseMat)
        {
            DynMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
            RadialImage->SetBrushFromMaterial(DynMaterial);
        }
    }
}

void UTestUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    FVector2D MousePosition;
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);

    if (PC && PC->GetMousePosition(MousePosition.X, MousePosition.Y))
    {
        FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(GetWorld());
        FVector2D CenterPosition = ViewportSize / 2.0f;

        FVector2D Direction = MousePosition - CenterPosition;
        float Distance = Direction.Size();

        float Deadzone = 50.0f;
        int32 Segments = 6;

        if (Distance > Deadzone)
        {
            float Angle = FMath::Atan2(Direction.Y, Direction.X) * (180.0f / PI);

            if (Angle < 0.0f)
            {
                Angle += 360.0f;
            }

            float OffsetAngle = Angle + (360.0f / Segments / 2.0f);
            if (OffsetAngle >= 360.0f)
            {
                OffsetAngle -= 360.0f;
            }

            int32 SelectedIndex = FMath::FloorToInt(OffsetAngle / (360.0f / Segments));

            if (SelectedIndex != PreviousIndex)
            {
                OnSelectedIndexChanged(SelectedIndex);
                PreviousIndex = SelectedIndex;
                if (DynMaterial)
                    DynMaterial->SetScalarParameterValue(TEXT("HighlightIndex"), (float)SelectedIndex);
            }
        }
        else
        {
            if (PreviousIndex != -1)
            {
                OnSelectedIndexChanged(-1);
                PreviousIndex = -1;
                if (DynMaterial)
                    DynMaterial->SetScalarParameterValue(TEXT("HighlightIndex"), -1.0f);
            }
        }
    }
}

EBlockForm UTestUI::IndexToForm(int32 Index) const
{
    switch (Index)
    {
    case 0: return EBlockForm::Ice;
    case 1: return EBlockForm::Rubber;
    case 2: return EBlockForm::Metal;
    case 3: return EBlockForm::Wood;
    case 4: return EBlockForm::Magnet;
    case 5: return EBlockForm::Copper;
    default: return EBlockForm::Ice;
    }
}

void UTestUI::ConfirmSelection()
{
    if (PreviousIndex < 0 || PreviousIndex > 5) return;
    if (!TargetActor) return;

    EBlockForm NewForm = IndexToForm(PreviousIndex);
    TargetActor->SetForm(NewForm);

    AmaterialCharacter* Player = Cast<AmaterialCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    if (Player)
    {
        FName Tag;
        switch (NewForm)
        {
        case EBlockForm::Rubber: Tag = TEXT("Rubber"); break;
        case EBlockForm::Metal:  Tag = TEXT("Metal");  break;
        case EBlockForm::Ice:    Tag = TEXT("Ice");     break;
        case EBlockForm::Wood:   Tag = TEXT("Wood");    break;
        case EBlockForm::Magnet: Tag = TEXT("Magnet");  break;
        case EBlockForm::Copper: Tag = TEXT("Copper");  break;
        default: return;
        }
        Player->DecreaseGaugeForMaterial(Tag);
    }
}