// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MilitaryStructureSubsystem.h"

#include "ProjectMMapWidget.generated.h"

class USceneCaptureComponent2D;

typedef TFunction< void(const FVector& /*EntityLocation*/, const bool& /*bIsOnTeam1*/, const FMassEntityHandle& /*Entity*/) > FMapDisplayableEntityFunction;

// Adapted from UCitySampleMapWidget.
UCLASS()
class PROJECTM_API UProjectMMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, meta = (BindWidget))
	class UCanvasPanel* CanvasPanel;

	UPROPERTY(EditAnywhere, meta = (BindWidget))
	class UBorder* Border;

	UPROPERTY(EditAnywhere, meta = (BindWidget))
	class UTextBlock* TextBlock_Team1Count;

	UPROPERTY(EditAnywhere, meta = (BindWidget))
	class UTextBlock* TextBlock_Team2Count;

	UFUNCTION(BlueprintCallable)
	void SetSelectedUnit(UMilitaryUnit* Unit);

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnSoldierButtonClicked(UMilitaryUnit* Unit);

	virtual void NativeOnInitialized() override;

	UPROPERTY(Transient, VisibleAnywhere)
	int32 CachedTeam1AliveSoldierCount;
	
	UPROPERTY(Transient, VisibleAnywhere)
	int32 CachedTeam2AliveSoldierCount;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	FVector2D WorldPositionToMapPosition(const FVector& WorldLocation);
	void InitializeMapViewProjectionMatrix(USceneCaptureComponent2D* const SceneCapture2D);
	void UpdateSoldierButtons();
	bool IsUnitChildOfSelectedUnit(UMilitaryUnit* Unit);
	void CreateSoldierButtons();
	class UButton* CreateButton(const FVector2D& Position, const FLinearColor& Color);
	void ForEachMapDisplayableEntity(const FMapDisplayableEntityFunction& EntityExecuteFunction);
	void UpdateSoldierCountLabels();

	/** Rect representing render target (map) space. */
	FIntRect MapRect;

	/** ViewProjection matrix used to project from world space to render target (map) space. */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Map Widget|Transient")
	FMatrix MapViewProjectionMatrix;

	bool bCreatedButtons = false;
	UMilitaryUnit* SelectedUnit = nullptr;
	TMap<UButton*, UMilitaryUnit*> ButtonToMilitaryUnitMap;
	UMilitaryStructureSubsystem* MilitaryStructureSubsystem;
};

// Static helper methods for Blueprints.
UCLASS()
class PROJECTM_API UProjectMMapWidgetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	static void RecursivelyExpandTreeViewUnitParents(class UTreeView* TreeView, UMilitaryUnit* Unit);
};
