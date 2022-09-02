#include "ProjectMWorldInfo.h"

#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"


AProjectMWorldInfo::AProjectMWorldInfo()
{
	PrimaryActorTick.bCanEverTick = false;

	WorldMapBoundingBox = CreateDefaultSubobject<UBoxComponent>("WorldMapBoundingBox");
	WorldMapBoundingBox->PrimaryComponentTick.bCanEverTick = false;
	WorldMapBoundingBox->SetGenerateOverlapEvents(false);
	WorldMapBoundingBox->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
	WorldMapBoundingBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WorldMapBoundingBox->bVisibleInReflectionCaptures = false;
	WorldMapBoundingBox->bVisibleInRealTimeSkyCaptures = false;
	WorldMapBoundingBox->bVisibleInRayTracing = false;
	WorldMapBoundingBox->bRenderInMainPass = false;
	WorldMapBoundingBox->bRenderInDepthPass = false;
	WorldMapBoundingBox->bReceivesDecals = false;
	WorldMapBoundingBox->bHiddenInSceneCapture = false;
	WorldMapBoundingBox->bReceiveMobileCSMShadows = false;

	RootComponent = WorldMapBoundingBox;

	WorldMapSceneCaptureComponent2D = CreateDefaultSubobject<USceneCaptureComponent2D>("WorldMapSceneCaptureComponent");
	WorldMapSceneCaptureComponent2D->PrimaryComponentTick.bCanEverTick = false;
	WorldMapSceneCaptureComponent2D->ProjectionType = ECameraProjectionMode::Orthographic;
	WorldMapSceneCaptureComponent2D->CaptureSource = SCS_BaseColor;
	WorldMapSceneCaptureComponent2D->bCaptureEveryFrame = false;
	WorldMapSceneCaptureComponent2D->bCaptureOnMovement = false;
	MapResolution = 1024;

	InitialSunlightYaw = 0.0f;
	SunlightYaw = 0.0f;
	SkyDomeYawOffset = 0.0f;
}

void AProjectMWorldInfo::BeginPlay()
{
	Super::BeginPlay();

	InitializeMapSceneCapture();
	InitializeSunlightSettings();
}

void AProjectMWorldInfo::InitializeMapSceneCapture()
{
	// If there is no bounding box or scene capture for initialization, do nothing
	if (!IsValid(WorldMapBoundingBox) || !IsValid(WorldMapSceneCaptureComponent2D))
	{
		return;
	}

	//Calculate bounds of the World
	const FVector& Center = WorldMapBoundingBox->GetComponentLocation();
	const FVector& Extents = WorldMapBoundingBox->GetScaledBoxExtent();
	FBox MapBounds(Center - Extents, Center + Extents);

	//Calculate Viewport size
	FBox2D WorldBounds2D(FVector2D(MapBounds.Min), FVector2D(MapBounds.Max));
	FVector2D ViewSize = WorldBounds2D.Max - WorldBounds2D.Min;
	float AspectRatio = FMath::Abs(ViewSize.X) / FMath::Abs(ViewSize.Y);
	uint32 ViewportWidth = MapResolution;
	uint32 ViewportHeight = MapResolution * AspectRatio;

	//Calculate Projection matrix based on world bounds.
	FMatrix CustomViewProjectionMatrix;
	AProjectMWorldInfo::CalculateTopWorldView(CustomViewProjectionMatrix, MapBounds, ViewportWidth, ViewportHeight);

	// Create a render target and set the world map scene capture
	if (!IsValid(WorldMapSceneCaptureComponent2D->TextureTarget))
	{
		UTextureRenderTarget2D* RenderTargetTexture = NewObject<UTextureRenderTarget2D>();
		RenderTargetTexture->ClearColor = FLinearColor::Transparent;
		RenderTargetTexture->TargetGamma = 2.2f;
		RenderTargetTexture->InitCustomFormat(ViewportWidth, ViewportHeight, PF_B8G8R8A8, false);
		RenderTargetTexture->UpdateResourceImmediate(true);
		WorldMapSceneCaptureComponent2D->TextureTarget = RenderTargetTexture;
	}

	float CaptureActorZ;
	if (WorldMapSceneCaptureComponent2D->ProjectionType == ECameraProjectionMode::Perspective)
	{
		// Calculate distance required for scene capture component to capture entire bounded area
		const float FOVAngle = WorldMapSceneCaptureComponent2D->FOVAngle;
		CaptureActorZ = Extents.Y / FMath::Tan(FMath::DegreesToRadians(FOVAngle * 0.5f));
	}
	else
	{
		CaptureActorZ = MapBounds.GetCenter().Z + MapBounds.GetExtent().Z;
		WorldMapSceneCaptureComponent2D->bUseCustomProjectionMatrix = true;
		WorldMapSceneCaptureComponent2D->CustomProjectionMatrix = CustomViewProjectionMatrix;
		WorldMapSceneCaptureComponent2D->OrthoWidth = ViewportWidth;
	}

	const FVector CaptureActorLocation(MapBounds.GetCenter().X, MapBounds.GetCenter().Y, CaptureActorZ);
	const FRotator CaptureActorRotation(-90.f, 0.f, 0.f);
	WorldMapSceneCaptureComponent2D->SetWorldLocationAndRotation(CaptureActorLocation, CaptureActorRotation);
	WorldMapSceneCaptureComponent2D->CaptureScene();
}

void AProjectMWorldInfo::CalculateTopWorldView(FMatrix& OutProjectionMatrix, const FBox& WorldBox, uint32 ViewportWidth, uint32 ViewportHeight)
{
	const FVector2D WorldSizeMin2D(WorldBox.Min.X, WorldBox.Min.Y);
	const FVector2D WorldSizeMax2D(WorldBox.Max.X, WorldBox.Max.Y);

	FVector2D WorldSize2D = (WorldSizeMax2D - WorldSizeMin2D);
	WorldSize2D.X = FMath::Abs(WorldSize2D.X);
	WorldSize2D.Y = FMath::Abs(WorldSize2D.Y);
	const bool bUseXAxis = (WorldSize2D.X / WorldSize2D.Y) > 1.f;
	const float WorldAxisSize = bUseXAxis ? WorldSize2D.X : WorldSize2D.Y;
	const uint32 ViewportAxisSize = bUseXAxis ? ViewportWidth : ViewportHeight;
	const float OrthoZoom = WorldAxisSize / ViewportAxisSize / 2.f;
	const float OrthoWidth = FMath::Max(1.f, ViewportWidth * OrthoZoom);
	const float OrthoHeight = FMath::Max(1.f, ViewportHeight * OrthoZoom);

	const float ZOffset = HALF_WORLD_MAX;
	OutProjectionMatrix = FReversedZOrthoMatrix(
		OrthoWidth,
		OrthoHeight,
		0.5f / ZOffset,
		ZOffset
	);

	ensureMsgf(!OutProjectionMatrix.ContainsNaN(), TEXT("Nans found on ProjectionMatrix"));
	if (OutProjectionMatrix.ContainsNaN())
	{
		OutProjectionMatrix.SetIdentity();
	}
}

void AProjectMWorldInfo::InitializeSunlightSettings()
{
	if (ADirectionalLight* const Light = SunLight.Get())
	{
		UDirectionalLightComponent* const LightComponent = Cast<UDirectionalLightComponent>(Light->GetLightComponent());
		check(LightComponent != nullptr);
		
		// Set the initial and current sunlight yaw using the current yaw of the directional "sun" light component
		InitialSunlightYaw = SunlightYaw = FRotator::ClampAxis(LightComponent->GetComponentRotation().Yaw);
	}

	if (AStaticMeshActor* const Sky = SkyDome.Get())
	{
		// Set the sky dome yaw offset using the current sunlight yaw and the set sky dome
		const float SkyDomeYaw = FRotator::ClampAxis(Sky->GetActorRotation().Yaw);
		SkyDomeYawOffset = SkyDomeYawOffset = FRotator::ClampAxis(SkyDomeYaw - SunlightYaw);
	}
}

void AProjectMWorldInfo::SetSunlightAngle(const float Angle)
{
	SunlightYaw = FRotator::ClampAxis(Angle);

	if (ADirectionalLight* const Light = SunLight.Get())
	{
		UDirectionalLightComponent* const LightComponent = Cast<UDirectionalLightComponent>(Light->GetLightComponent());
		check(LightComponent != nullptr);

		const FRotator& SunlightRotation = LightComponent->GetComponentRotation();
		LightComponent->SetWorldRotation(FRotator(SunlightRotation.Pitch, SunlightYaw, SunlightRotation.Roll));
	}

	if (AStaticMeshActor* const Sky = SkyDome.Get())
	{
		const FRotator& SkyRotation = Sky->GetActorRotation();
		const float SkyDomeYaw = FRotator::ClampAxis(SunlightYaw + SkyDomeYawOffset);
		Sky->SetActorRotation(FRotator(SkyRotation.Pitch, SkyDomeYaw, SkyRotation.Roll));
	}
}

void AProjectMWorldInfo::ResetSunlightAngle()
{
	SetSunlightAngle(InitialSunlightYaw);
}

float AProjectMWorldInfo::GetPostProcessingFilterBlendWeight() const
{
	return PostProcessingFilter.IsValid() ? PostProcessingFilter->BlendWeight : 0.0f;
}

void AProjectMWorldInfo::SetPostProcessingFilterBlendWeight(const float BlendWeight)
{
	if (PostProcessingFilter.IsValid())
	{
		PostProcessingFilter->BlendWeight = FMath::Clamp(BlendWeight, 0.0f, 1.0f);
	}
}
