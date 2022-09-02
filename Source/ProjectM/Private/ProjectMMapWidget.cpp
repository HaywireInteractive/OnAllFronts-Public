// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectMMapWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBoxSlot.h"


static const float GButtonSize = 10.f;

void UProjectMMapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
  Super::NativeTick(MyGeometry, InDeltaTime);

  if (!CanvasPanel)
  {
    return;
  }

  const FVector2D& WidgetSize = MyGeometry.GetLocalSize();

  auto ChildCount = CanvasPanel->GetChildrenCount();
  if (ChildCount > 0)
  {
    const TArray<UPanelSlot*>& PanelSlots = CanvasPanel->GetSlots();
    for (int i = 0; i < PanelSlots.Num(); i++)
    {
      UCanvasPanelSlot* ButtonSlot = Cast<UCanvasPanelSlot>(PanelSlots[i]);
      if (ButtonSlot)
      {
        auto Position = ButtonSlot->GetPosition();
        auto newX = Position.X + 1.f > WidgetSize.X ? 0.f : Position.X + 1.f;
        ButtonSlot->SetPosition(FVector2D(newX, Position.Y));
      }
    }

    return;
  }


  for (int x = 0; x < 100; x++)
  {
    for (int y = 0; y < 100; y++)
    {
      UButton* Button = NewObject<UButton>();
      Button->WidgetStyle.Normal.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
      Button->WidgetStyle.Normal.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
      Button->WidgetStyle.Normal.TintColor = FLinearColor(1.f, 0.f, 0.f);

      UCanvasPanelSlot* CanvasPanelSlot = CanvasPanel->AddChildToCanvas(Button);
      CanvasPanelSlot->SetPosition(FVector2D((x / 100.f) * WidgetSize.X, (y / 100.f) * WidgetSize.Y));
      CanvasPanelSlot->SetSize(FVector2D(GButtonSize, GButtonSize));
    }
  }
}
