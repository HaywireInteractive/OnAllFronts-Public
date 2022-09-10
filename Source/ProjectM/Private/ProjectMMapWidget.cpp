// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectMMapWidget.h"

#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/TreeView.h"
#include "Components/SceneCaptureComponent2D.h"
#include "MassEntitySubsystem.h"
#include "MassEntityQuery.h"
#include "ProjectMWorldInfo.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/TextureRenderTarget2D.h"
#include "MassCommonFragments.h"
#include "MassEnemyTargetFinderProcessor.h"

#include <Character/CommanderCharacter.h>

#define LOCTEXT_NAMESPACE "MyNamespace" // TODO

static const float GSoldierButtonSize = 10.f;
static const float GTankButtonSize = 20.f;

// TODO: don't hard-code
static const FLinearColor GSelectedUnitColor = FLinearColor(0.f, 1.f, 0.f);
static const FLinearColor GPlayerSoldierColor = FLinearColor(1.f, 1.f, 0.f);
static const FLinearColor GTeamColors[] = {
  FLinearColor(0.f, 0.f, 1.f),
  FLinearColor(1.f, 0.f, 0.f),
};

//----------------------------------------------------------------------//
//  UProjectMMapWidget
//----------------------------------------------------------------------//
void UProjectMMapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
  Super::NativeTick(MyGeometry, InDeltaTime);

  if (!CanvasPanel)
  {
    return;
  }

  if (!bCreatedButtons)
  {
    CreateMapButtons();
  }
  else
  {
    UpdateMapButtons();
  }

  UpdateSoldierCountLabels();
}

void UProjectMMapWidget::UpdateSoldierCountLabels()
{
  if (TextBlock_Team1Count)
  {
    TextBlock_Team1Count->SetText(FText::Format(LOCTEXT("TODO", "Team 1: {0}"), CachedTeam1AliveSoldierCount));
  }
  if (TextBlock_Team2Count)
  {
    TextBlock_Team2Count->SetText(FText::Format(LOCTEXT("TODO", "Team 2: {0}"), CachedTeam2AliveSoldierCount));
  }
}

void UProjectMMapWidget::CreateMapButtons()
{
  ForEachMapDisplayableEntity([this](const FVector& EntityLocation, const bool& bIsOnTeam1, const bool& bIsPlayer, const FMassEntityHandle& Entity)
  {
    UMilitaryUnit* Unit = MilitaryStructureSubsystem->GetUnitForEntity(Entity);

    if (!Unit)
    {
      UE_LOG(LogTemp, Warning, TEXT("UProjectMMapWidget: Could not find UMilitaryUnit for entity, not creating button"));
      return;
    }

    FLinearColor Color = GTeamColors[bIsOnTeam1];
    (bIsOnTeam1 ? CachedTeam1AliveSoldierCount : CachedTeam2AliveSoldierCount)++;
    UButton* Button = CreateButton(Unit->bIsSoldier);
    UpdateButton(Button, WorldPositionToMapPosition(EntityLocation), Unit, bIsOnTeam1, bIsPlayer);
    ButtonToMilitaryUnitMap.Add(Button, Unit);
    MilitaryUnitToButtonMap.Add(Unit, Button);
  });

  bCreatedButtons = true;
}

void UProjectMMapWidget::ForEachMapDisplayableEntity(const FMapDisplayableEntityFunction& EntityExecuteFunction)
{
  UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(GetWorld());
  FMassEntityQuery EntityQuery;
  EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
  EntityQuery.AddRequirement<FTeamMemberFragment>(EMassFragmentAccess::ReadOnly);
  FMassExecutionContext Context(0.0f);

  EntityQuery.ForEachEntityChunk(*EntitySubsystem, Context, [&EntityExecuteFunction](FMassExecutionContext& Context)
  {
    const int32 NumEntities = Context.GetNumEntities();

    const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
    const TConstArrayView<FTeamMemberFragment> TeamMemberList = Context.GetFragmentView<FTeamMemberFragment>();

    for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
    {
      const bool& bIsPlayer = Context.DoesArchetypeHaveTag<FMassPlayerControllableCharacterTag>();
      EntityExecuteFunction(TransformList[EntityIndex].GetTransform().GetLocation(), TeamMemberList[EntityIndex].IsOnTeam1, bIsPlayer, Context.GetEntity(EntityIndex));
    }
  });
}

UButton* UProjectMMapWidget::CreateButton(const bool& bIsSolder)
{
  UButton* Button = NewObject<UButton>();

  if (bIsSolder)
  {
    Button->WidgetStyle.Normal.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
    Button->WidgetStyle.Normal.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
  }

  UCanvasPanelSlot* CanvasPanelSlot = CanvasPanel->AddChildToCanvas(Button);
  CanvasPanelSlot->SetAnchors(FAnchors(0.5f));
  CanvasPanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
  const float& ButtonSize = bIsSolder ? GSoldierButtonSize : GTankButtonSize;
  CanvasPanelSlot->SetSize(FVector2D(ButtonSize, ButtonSize));

  SButton* ButtonWidget = (SButton*)&(Button->TakeWidget().Get());
  ButtonWidget->SetOnClicked(FOnClicked::CreateLambda([this, Button]()
  {
    if (ButtonToMilitaryUnitMap.Contains(Button))
    {
      BP_OnSoldierButtonClicked(ButtonToMilitaryUnitMap[Button]);
    }
    return FReply::Handled();
  }));

  return Button;
}

void UProjectMMapWidget::UpdateButton(UButton* Button, const FVector2D& Position, UMilitaryUnit* Unit, const bool& bIsOnTeam1, const bool& bIsPlayer)
{
  Button->WidgetStyle.Normal.TintColor = bIsPlayer ? GPlayerSoldierColor : (Unit->IsChildOfUnit(SelectedUnit) ? GSelectedUnitColor : GTeamColors[bIsOnTeam1]);

  UCanvasPanelSlot* CanvasPanelSlot = CastChecked<UCanvasPanelSlot>(Button->Slot.Get());
  CanvasPanelSlot->SetPosition(Position);
}

void UProjectMMapWidget::UpdateMapButtons()
{
  CachedTeam1AliveSoldierCount = CachedTeam2AliveSoldierCount = 0;

  TSet<UButton*> UpdatedButtons;
  ForEachMapDisplayableEntity([this, &UpdatedButtons](const FVector& EntityLocation, const bool& bIsOnTeam1, const bool& bIsPlayer, const FMassEntityHandle& Entity)
  {
    (bIsOnTeam1 ? CachedTeam1AliveSoldierCount : CachedTeam2AliveSoldierCount)++;
    UMilitaryUnit* Unit = MilitaryStructureSubsystem->GetUnitForEntity(Entity);
    UButton* Button = MilitaryUnitToButtonMap[Unit];
    UpdateButton(Button, WorldPositionToMapPosition(EntityLocation), Unit, bIsOnTeam1, bIsPlayer);
    UpdatedButtons.Add(Button);
    UCanvasPanelSlot* ButtonSlot = CastChecked<UCanvasPanelSlot>(Button->Slot.Get());
    const FVector2D& MapPosition = WorldPositionToMapPosition(EntityLocation);
    ButtonSlot->SetPosition(MapPosition);
  });

  // Hide buttons for destroyed units.
  for (int32 CanvasPanelChildIndex = 0; CanvasPanelChildIndex < CanvasPanel->GetChildrenCount(); CanvasPanelChildIndex++)
  {
    UButton* Button = Cast<UButton>(CanvasPanel->GetChildAt(CanvasPanelChildIndex));
    if (Button && !UpdatedButtons.Contains(Button))
    {
      Button->SetVisibility(ESlateVisibility::Collapsed);
    }
  }
}

FVector2D UProjectMMapWidget::WorldPositionToMapPosition(const FVector& WorldLocation)
{
  FVector2D MapPosition;
  FSceneView::ProjectWorldToScreen(WorldLocation, MapRect, MapViewProjectionMatrix, MapPosition);
  return MapPosition;
}

FVector UProjectMMapWidget::MapPositionToWorldPosition(const FVector2D& MapPosition) const
{
  FVector WorldPosition, WorldDirection;
  FSceneView::DeprojectScreenToWorld(MapPosition, MapRect, MapViewProjectionMatrix.Inverse(), WorldPosition, WorldDirection);
  return WorldPosition;
}

void UProjectMMapWidget::NativeOnInitialized()
{
  Super::NativeOnInitialized();

  MilitaryStructureSubsystem = UWorld::GetSubsystem<UMilitaryStructureSubsystem>(GetWorld());
  check(MilitaryStructureSubsystem);

  const AProjectMWorldInfo* const WorldInfo = Cast<AProjectMWorldInfo>(UGameplayStatics::GetActorOfClass(GetWorld(), AProjectMWorldInfo::StaticClass()));
  USceneCaptureComponent2D* const SceneCapture = WorldInfo ? WorldInfo->GetWorldMapSceneCapture() : nullptr;

  if (!SceneCapture)
  {
    return;
  }

  InitializeMapViewProjectionMatrix(SceneCapture);

  if (UTextureRenderTarget2D* const Texture = SceneCapture->TextureTarget)
  {
    const FIntPoint MapBaseSize = { Texture->SizeX / 2, Texture->SizeY / 2 };
    MapRect = { -MapBaseSize.X, -MapBaseSize.Y, MapBaseSize.X, MapBaseSize.Y };

    if (Border)
    {
      UCanvasPanelSlot* BorderSlot = CastChecked<UCanvasPanelSlot>(Border->Slot.Get());
      BorderSlot->SetSize(MapRect.Size());
    }
  }
}

void UProjectMMapWidget::OnHide()
{
  for (auto& KeyValuePair : ButtonToMilitaryUnitMap)
  {
    UButton* Button = KeyValuePair.Key;
    Button->RemoveFromParent();
  }

  ButtonToMilitaryUnitMap.Reset();
  MilitaryUnitToButtonMap.Reset();
  bCreatedButtons = false;
}

void UProjectMMapWidget::InitializeMapViewProjectionMatrix(USceneCaptureComponent2D* const SceneCapture2D)
{
  check(SceneCapture2D);

  // Cache the MapViewProjection matrix for world to render target (map) space projections
  FMinimalViewInfo InMapViewInfo;
  SceneCapture2D->GetCameraView(0.0f, InMapViewInfo);

  // Get custom projection matrix, if it exists
  TOptional<FMatrix> InCustomProjectionMatrix;
  if (SceneCapture2D->bUseCustomProjectionMatrix)
  {
    InCustomProjectionMatrix = SceneCapture2D->CustomProjectionMatrix;
  }

  // The out parameters for the individual view and projection matrix will not be needed
  FMatrix MapViewMatrix, MapProjectionMatrix;

  // Cache the MapViewProjection matrix
  UGameplayStatics::CalculateViewProjectionMatricesFromMinimalView(
    InMapViewInfo,
    InCustomProjectionMatrix,
    MapViewMatrix,
    MapProjectionMatrix,
    MapViewProjectionMatrix
  );
}

void UProjectMMapWidget::SetSelectedUnit(UMilitaryUnit* Unit)
{
  SelectedUnit = Unit;
}

UCanvasPanel* UProjectMMapWidget::GetCanvasPanel() const
{
  return CanvasPanel;
}

UBorder* UProjectMMapWidget::GetBorder() const
{
  return Border;
}

//----------------------------------------------------------------------//
//  UProjectMMapWidgetLibrary
//----------------------------------------------------------------------//
void UProjectMMapWidgetLibrary::RecursivelyExpandTreeViewUnitParents(UTreeView* TreeView, UMilitaryUnit* Unit)
{
  while (Unit)
  {
    TreeView->SetItemExpansion(Unit, true);
    Unit = Unit->Parent;
  }
}
