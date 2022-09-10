// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MilitaryStructureSubsystem.h"

#include "ProjectMMapWidget.generated.h"

class USceneCaptureComponent2D;

typedef TFunction< void(const FVector& /*EntityLocation*/, const bool& /*bIsOnTeam1*/, const bool& /*bIsPlayer*/, const FMassEntityHandle& /*Entity*/) > FMapDisplayableEntityFunction;

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

	UFUNCTION(BlueprintCallable)
	void OnHide();

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnSoldierButtonClicked(UMilitaryUnit* Unit);

	virtual void NativeOnInitialized() override;

	UPROPERTY(Transient, VisibleAnywhere)
	int32 CachedTeam1AliveSoldierCount;
	
	UPROPERTY(Transient, VisibleAnywhere)
	int32 CachedTeam2AliveSoldierCount;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable)
	class UCanvasPanel* GetCanvasPanel() const;

	UFUNCTION(BlueprintCallable)
	class UBorder* GetBorder() const;

	UFUNCTION(BlueprintCallable)
	FVector MapPositionToWorldPosition(const FVector2D& MapPosition) const;

private:
	FVector2D WorldPositionToMapPosition(const FVector& WorldLocation);
	void InitializeMapViewProjectionMatrix(USceneCaptureComponent2D* const SceneCapture2D);
	void CreateMapButtons();
	void UpdateMapButtons();
	class UButton* CreateButton(const bool& bIsSolder);
	void UpdateButton(class UButton* Button, const FVector2D& Position, UMilitaryUnit* Unit, const bool& bIsOnTeam1, const bool& bIsPlayer);
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
	TMap<UMilitaryUnit*, UButton*> MilitaryUnitToButtonMap;
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
