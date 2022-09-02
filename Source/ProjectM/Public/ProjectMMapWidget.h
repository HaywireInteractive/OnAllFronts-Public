// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ProjectMMapWidget.generated.h"

class USceneCaptureComponent2D;

typedef TFunction< void(const FVector& /*EntityLocation*/, const bool& /*bIsOnTeam1*/) > FMapDisplayableEntityFunction;

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

	virtual void NativeOnInitialized() override;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	FVector2D WorldPositionToMapPosition(const FVector& WorldLocation);
	void InitializeMapViewProjectionMatrix(USceneCaptureComponent2D* const SceneCapture2D);
	void UpdateSoldierButtons();
	void CreateSoldierButtons();
	void CreateButton(const FVector2D& Position, const FLinearColor& Color);
	void ForEachMapDisplayableEntity(const FMapDisplayableEntityFunction& EntityExecuteFunction);

	/** Rect representing render target (map) space. */
	FIntRect MapRect;

	/** ViewProjection matrix used to project from world space to render target (map) space. */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Map Widget|Transient")
	FMatrix MapViewProjectionMatrix;

	bool bCreatedButtons = false;
};
