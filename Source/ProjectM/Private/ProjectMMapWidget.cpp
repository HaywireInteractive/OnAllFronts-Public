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
#include "MassMapTranslatorProcessor.h"
#include "ProjectMWorldInfo.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/TextureRenderTarget2D.h"
#include "MassCommonFragments.h"
#include "MassEnemyTargetFinderProcessor.h"

#define LOCTEXT_NAMESPACE "MyNamespace" // TODO

static const float GButtonSize = 10.f;

// TODO: don't hard-code
static const FLinearColor GSelectedUnitColor = FLinearColor(0.f, 1.f, 0.f);
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
    CreateSoldierButtons();
  }
  else
  {
    UpdateSoldierButtons();
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

void UProjectMMapWidget::CreateSoldierButtons()
{
  ForEachMapDisplayableEntity([this](const FVector& EntityLocation, const bool& bIsOnTeam1, const FMassEntityHandle& Entity)
  {
    FLinearColor Color = GTeamColors[bIsOnTeam1];
    (bIsOnTeam1 ? CachedTeam1AliveSoldierCount : CachedTeam2AliveSoldierCount)++;
    UMilitaryUnit* Unit = MilitaryStructureSubsystem->GetUnitForEntity(Entity);
    UButton* Button = CreateButton(WorldPositionToMapPosition(EntityLocation), Color);
    ButtonToMilitaryUnitMap.Add(Button, Unit);
  });

  bCreatedButtons = true;
}

void UProjectMMapWidget::ForEachMapDisplayableEntity(const FMapDisplayableEntityFunction& EntityExecuteFunction)
{
  UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(GetWorld());
  FMassEntityQuery EntityQuery;
  EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
  EntityQuery.AddRequirement<FTeamMemberFragment>(EMassFragmentAccess::ReadOnly);
  EntityQuery.AddTagRequirement<FMassMapDisplayableTag>(EMassFragmentPresence::All);
  FMassExecutionContext Context(0.0f);

  EntityQuery.ForEachEntityChunk(*EntitySubsystem, Context, [&EntityExecuteFunction](FMassExecutionContext& Context)
  {
    const int32 NumEntities = Context.GetNumEntities();

    const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
    const TConstArrayView<FTeamMemberFragment> TeamMemberList = Context.GetFragmentView<FTeamMemberFragment>();

    for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
    {
      EntityExecuteFunction(TransformList[EntityIndex].GetTransform().GetLocation(), TeamMemberList[EntityIndex].IsOnTeam1, Context.GetEntity(EntityIndex));
    }
  });
}

UButton* UProjectMMapWidget::CreateButton(const FVector2D& Position, const FLinearColor& Color)
{
  UButton* Button = NewObject<UButton>();
  Button->WidgetStyle.Normal.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
  Button->WidgetStyle.Normal.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
  Button->WidgetStyle.Normal.TintColor = Color;

  UCanvasPanelSlot* CanvasPanelSlot = CanvasPanel->AddChildToCanvas(Button);
  CanvasPanelSlot->SetAnchors(FAnchors(0.5f));
  CanvasPanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
  CanvasPanelSlot->SetPosition(Position);
  CanvasPanelSlot->SetSize(FVector2D(GButtonSize, GButtonSize));

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

void UProjectMMapWidget::UpdateSoldierButtons()
{
  CachedTeam1AliveSoldierCount = CachedTeam2AliveSoldierCount = 0;
  int32 ButtonIndex = 0;

  // Find first button index.
  // TODO: this is brittle in case not all buttons are at end of CanvasPanel's children. Find safer way to do this.
  for (; ButtonIndex < CanvasPanel->GetChildrenCount(); ButtonIndex++)
  {
    UButton* Button = Cast<UButton>(CanvasPanel->GetChildAt(ButtonIndex));
    if (Button)
    {
      break;
    }
  }

  ForEachMapDisplayableEntity([&ButtonIndex, this](const FVector& EntityLocation, const bool& bIsOnTeam1, const FMassEntityHandle& Entity)
  {
    (bIsOnTeam1 ? CachedTeam1AliveSoldierCount : CachedTeam2AliveSoldierCount)++;
    UMilitaryUnit* Unit = MilitaryStructureSubsystem->GetUnitForEntity(Entity);
    UButton* Button = CastChecked<UButton>(CanvasPanel->GetChildAt(ButtonIndex++));
    Button->WidgetStyle.Normal.TintColor = IsUnitChildOfSelectedUnit(Unit) ? GSelectedUnitColor : GTeamColors[bIsOnTeam1];
    UCanvasPanelSlot* ButtonSlot = CastChecked<UCanvasPanelSlot>(Button->Slot.Get());
    const FVector2D& MapPosition = WorldPositionToMapPosition(EntityLocation);
    ButtonSlot->SetPosition(MapPosition);

    ButtonToMilitaryUnitMap[Button] = Unit;
  });

  // Hide remaining buttons.
  for (; ButtonIndex < CanvasPanel->GetChildrenCount(); ButtonIndex++)
  {
    UButton* Button = CastChecked<UButton>(CanvasPanel->GetChildAt(ButtonIndex));
    Button->SetVisibility(ESlateVisibility::Collapsed);
  }
}

bool UProjectMMapWidget::IsUnitChildOfSelectedUnit(UMilitaryUnit* Unit)
{
  if (!SelectedUnit) {
    return false;
  }

  if (SelectedUnit->bIsSoldier)
  {
    return Unit == SelectedUnit;
  }

  // If we got here, SelectedUnit is set to a non-soldier.

  while (Unit)
  {
    if (Unit == SelectedUnit) {
      return true;
    }
    Unit = Unit->Parent;
  }

  return false;
}

FVector2D UProjectMMapWidget::WorldPositionToMapPosition(const FVector& WorldLocation)
{
  FVector2D MapPosition;
  FSceneView::ProjectWorldToScreen(WorldLocation, MapRect, MapViewProjectionMatrix, MapPosition);
  return MapPosition;
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

void UProjectMMapWidgetLibrary::SetListItemSelectedWithOnItemSelectedChangedEvent(UListView* ListView, TScriptInterface<IUserListEntry> UserListEntry)
{
  UUserWidget* ListEntryWidget = Cast<UUserWidget>(UserListEntry.GetObject());
  if (!ListEntryWidget || !ListView)
  {
    return;
  }
  auto PreviousSelectedItemEntryWidget = ListView->GetEntryWidgetFromItem(ListView->GetSelectedItem());
  if (PreviousSelectedItemEntryWidget)
  {
    IUserListEntry::UpdateItemSelection(*PreviousSelectedItemEntryWidget, false);
  }
  IUserListEntry::UpdateItemSelection(*ListEntryWidget, true);
}