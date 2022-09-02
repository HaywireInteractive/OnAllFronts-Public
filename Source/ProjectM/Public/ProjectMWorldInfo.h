#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "ProjectMWorldInfo.generated.h"

class UBoxComponent;
class USceneCaptureComponent2D;
class ADirectionalLight;
class AStaticMeshActor;
class APostProcessVolume;

/**
 * Actor for storing world data and whose bounds represent the world bounds. Adapted from ACitySampleWorldInfo.
 */
UCLASS(Blueprintable)
class PROJECTM_API AProjectMWorldInfo : public AInfo
{
	GENERATED_BODY()

public:
	AProjectMWorldInfo();

	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	//~ End AActor Interface

	//////////////////////////////////////////////////////////////////////////
	// World Map

public:
	UFUNCTION(BlueprintPure, Category = "UI|World Map")
	UBoxComponent* GetWorldMapBounds() const
	{
		return WorldMapBoundingBox;
	}

	UFUNCTION(BlueprintPure, Category = "UI|World Map")
	USceneCaptureComponent2D* GetWorldMapSceneCapture() const
	{
		return WorldMapSceneCaptureComponent2D;
	}

protected:	
	/** Bounding box representing the world map bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|World Map")
	UBoxComponent* WorldMapBoundingBox;

	/** SceneCapture2D actor whose render target is used by the UI to represent the World Map. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|World Map")
	USceneCaptureComponent2D* WorldMapSceneCaptureComponent2D;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|World Map")
	int32 MapResolution;

private:
	void InitializeMapSceneCapture();
	void CalculateTopWorldView(FMatrix& OutProjectionMatrix, const FBox& WorldBox, uint32 ViewportWidth, uint32 ViewportHeight);

	//////////////////////////////////////////////////////////////////////////
	// Sun Rotation

public:
	/** Sets the sunlight settings using the angle of the directional light and sky dome. */
	UFUNCTION(BlueprintCallable, Category = "Sim|Sunlight")
	void InitializeSunlightSettings();

	UFUNCTION(BlueprintPure, Category = "Sim|Sunlight")
	float GetSunlightAngle() const
	{
		return SunlightYaw;
	}

	/** Sets the directional "sun" light and sky dome to match the given angle. */
	UFUNCTION(BlueprintCallable, Category = "Sim|Sunlight")
	void SetSunlightAngle(const float Angle);

	/** Resets the directional "sun" light and sky dome to match the initial angles. */
	UFUNCTION(BlueprintCallable, Category = "Sim|Sunlight")
	void ResetSunlightAngle();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sim|Sunlight")
	TSoftObjectPtr<ADirectionalLight> SunLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sim|Sunlight")
	TSoftObjectPtr<AStaticMeshActor> SkyDome;

private:
	/** Initial yaw set during InitializeSunlightSettings using the directional "sun" light. */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Sim|Sunlight")
	float InitialSunlightYaw;
	
	/** The yaw of the "sun" light last set by SetSunlightAngle. */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Sim|Sunlight")
	float SunlightYaw;

	/** Offset to align the sky dome "sun" with the sunlight direction. */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Sim|Sunlight")
	float SkyDomeYawOffset;

	//////////////////////////////////////////////////////////////////////////
	// Post-Processing Filter

public:
	UFUNCTION(BlueprintPure, Category="Sim|Post-Processing Filter")
	float GetPostProcessingFilterBlendWeight() const;

	UFUNCTION(BlueprintCallable, Category="Sim|Post-Processing Filter")
	void SetPostProcessingFilterBlendWeight(const float BlendWeight);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sim|Post-Processing Filter")
	TSoftObjectPtr<APostProcessVolume> PostProcessingFilter;
};
