/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-13 01:51:05
* @Last Modified by:   feng_ye
* @Last Modified time: 2023-07-26 16:05:21
*/


#include "EvercoastVoxelRendererComp.h"
#include "EvercoastLocalVoxelFrame.h"
#include "EvercoastInstancedCubeVertexFactory.h"
#include "EvercoastVoxelSceneProxy.h"
#include "EvercoastBasicStreamingDataUploader.h"
#include "Engine/TextureRenderTarget2D.h"
#include "MaterialShared.h"
#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 2
#include "MaterialDomain.h"
#endif
#endif


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// UEvercoastVoxelRendererComp
///////////////////////////////////////////////////////////////////////////////////////////////////////////

UEvercoastVoxelRendererComp::UEvercoastVoxelRendererComp(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	/*
	bHiddenInGame = false;
	bUseEditorCompositing = true;
	SetGenerateOverlapEvents(false);
	*/
	CastShadow = true; // default has shadows
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true; // need this to tick and subsequently call MarkRenderTransformDirty()
	bUseAttachParentBound = false;
	m_dirtyMark = true;
	m_voxelUploader = std::make_shared<EvercoastBasicStreamingDataUploader>(this);
}

std::shared_ptr<IEvercoastStreamingDataUploader> UEvercoastVoxelRendererComp::GetVoxelDataUploader() const
{
	return m_voxelUploader;
}

void UEvercoastVoxelRendererComp::MarkDirty()
{
	m_dirtyMark = true;
}

/// <summary>
/// UEvercoastVoxelRendererComp::CreateSceneProxy()
/// </summary>
/// <returns>New scene proxy for Evercoast voxel content</returns>

FPrimitiveSceneProxy* UEvercoastVoxelRendererComp::CreateSceneProxy()
{
	if (!SceneProxy)
	{
		// whenever creates a new scene proxy, we mark both the voxel data and transform/bounds data dirty so that they gets updated in Tick()
		MarkDirty();
		m_voxelUploader->MarkDataDirty();

		if (VoxelMaterial)
			VoxelMaterialDynamic = CreateAndSetMaterialInstanceDynamicFromMaterial(0, VoxelMaterial);
		else
			VoxelMaterialDynamic = CreateAndSetMaterialInstanceDynamicFromMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));

		if (bGenerateNormal)
		{
			for (int i = 0; i < 2; ++i)
			{
				if (!CaptureWorldNormalRenderTarget[i])
				{
					CaptureWorldNormalRenderTarget[i] = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
					CaptureWorldNormalRenderTarget[i]->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA16f;
					CaptureWorldNormalRenderTarget[i]->InitAutoFormat(GenerateWorldNormalSize, GenerateWorldNormalSize);
					CaptureWorldNormalRenderTarget[i]->UpdateResourceImmediate(true);
				}
			}
		}

		bool shouldUseOverride = OverrideWorldNormalRenderTarget[0] != nullptr && OverrideWorldNormalRenderTarget[1] != nullptr;

		if (bGenerateNormal && !shouldUseOverride)
		{
			for (int i = 0; i < 2; ++i)
			{
				if (!FilteredWorldNormalRenderTarget[i])
				{
					FilteredWorldNormalRenderTarget[i] = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
					FilteredWorldNormalRenderTarget[i]->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA16f;
					FilteredWorldNormalRenderTarget[i]->InitAutoFormat(FilteredWorldNormalSize, FilteredWorldNormalSize);
					FilteredWorldNormalRenderTarget[i]->UpdateResourceImmediate(true);
				}
			}
		}

		UTextureRenderTarget2D* normalRenderTarget_Left, * normalRenderTarget_Right;
		if (shouldUseOverride)
		{
			normalRenderTarget_Left = OverrideWorldNormalRenderTarget[0];
			normalRenderTarget_Right = OverrideWorldNormalRenderTarget[1];
		}
		else
		{
			normalRenderTarget_Left = FilteredWorldNormalRenderTarget[0];
			normalRenderTarget_Right = FilteredWorldNormalRenderTarget[1];
		}

		bool canGenerateNormal = bGenerateNormal && CaptureWorldNormalRenderTarget[0] != nullptr && CaptureWorldNormalRenderTarget[1] != nullptr &&
			CaptureWorldNormalRenderTarget[0]->GetSurfaceWidth() == CaptureWorldNormalRenderTarget[1]->GetSurfaceWidth() &&
			CaptureWorldNormalRenderTarget[0]->GetSurfaceHeight() == CaptureWorldNormalRenderTarget[1]->GetSurfaceHeight() &&
			normalRenderTarget_Left != nullptr && normalRenderTarget_Right != nullptr &&
			normalRenderTarget_Left->GetSurfaceWidth() == normalRenderTarget_Right->GetSurfaceWidth() &&
			normalRenderTarget_Left->GetSurfaceHeight() == normalRenderTarget_Right->GetSurfaceHeight() &&
			normalRenderTarget_Left->GetFormat() == normalRenderTarget_Right->GetFormat();

		auto newSceneProxy = new FEvercoastVoxelSceneProxy(this, VoxelMaterialDynamic, canGenerateNormal, bUseIcosahedronForNormalGeneration, NormalVoxelSizeFactor, 
			WorldNormalSmoothIteration, CaptureWorldNormalRenderTarget[0], CaptureWorldNormalRenderTarget[1], normalRenderTarget_Left, normalRenderTarget_Right);
		ENQUEUE_RENDER_COMMAND(FEvercoastVoxelDataUpdate)(
			[sceneProxy=newSceneProxy, voxelFrame=m_currLocalVoxelFrame](FRHICommandListImmediate& RHICmdList)
			{
				sceneProxy->SetVoxelData_RenderThread(RHICmdList, voxelFrame);
			});

		return newSceneProxy;
	}

	return SceneProxy;
}

int32 UEvercoastVoxelRendererComp::GetNumMaterials() const
{
	if (VoxelMaterialDynamic)
		return 1;

	return 0;
}

UMaterialInterface* UEvercoastVoxelRendererComp::GetMaterial(int32 ElementIndex) const
{
	if (VoxelMaterial)
		return VoxelMaterial; // need to return the "vanilla" material to be able to create dynamic material instance

	return UMaterial::GetDefaultMaterial(MD_Surface);
}

void UEvercoastVoxelRendererComp::GetUsedMaterials(TArray< UMaterialInterface* >& OutMaterials, bool bGetDebugMaterials) const
{
	if (VoxelMaterialDynamic)
		OutMaterials.Add(VoxelMaterialDynamic);
}

/// <summary>
/// Calculate bounding box/sphere for inherited USceneComponent
/// </summary>
/// <param name="LocalToWorld"></param>
/// <returns>A bounding structure of the content</returns>
FBoxSphereBounds UEvercoastVoxelRendererComp::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds bounds = CalcLocalBounds();

	return bounds.TransformBy(LocalToWorld);
}

FBoxSphereBounds UEvercoastVoxelRendererComp::CalcLocalBounds() const
{
	if (m_currLocalVoxelFrame)
	{
		return m_currLocalVoxelFrame->CalcBounds();
	}

	return FEvercoastVoxelSceneProxy::GetDefaultVoxelDataBounds();
}

void UEvercoastVoxelRendererComp::SetVoxelData(std::shared_ptr<EvercoastLocalVoxelFrame> localVoxelFrame)
{
	m_currLocalVoxelFrame = localVoxelFrame;

	std::shared_ptr<EvercoastLocalVoxelFrame> voxelFrame = localVoxelFrame;
	FEvercoastVoxelSceneProxy* sceneProxy = (FEvercoastVoxelSceneProxy*)(this->SceneProxy);
	
	ENQUEUE_RENDER_COMMAND(FEvercoastVoxelDataUpdate)(
		[sceneProxy, voxelFrame](FRHICommandListImmediate& RHICmdList)
		{
			sceneProxy->SetVoxelData_RenderThread(RHICmdList, voxelFrame);
		});

	if (!VoxelMaterialDynamic)
	{
		if (VoxelMaterial)
			VoxelMaterialDynamic = CreateAndSetMaterialInstanceDynamicFromMaterial(0, VoxelMaterial);
		else
			VoxelMaterialDynamic = CreateAndSetMaterialInstanceDynamicFromMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));

		if (SceneProxy)
		{
			((FEvercoastVoxelSceneProxy*)SceneProxy)->ResetMaterial(VoxelMaterialDynamic);
		}
	}

	VoxelMaterialDynamic->SetScalarParameterValue(TEXT("WorldNormalFactor"), bGenerateNormal ? 1.0f : 0.0f);

	m_dirtyMark = true;
}


void UEvercoastVoxelRendererComp::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (m_voxelUploader->IsDataDirty())
	{
		m_voxelUploader->ForceUpload();
	}

	if (m_dirtyMark)
	{
		UpdateBounds();
		
		MarkRenderTransformDirty();
		m_dirtyMark = false;
	}
}

void UEvercoastVoxelRendererComp::SetVoxelMaterial(UMaterialInterface* newMaterial)
{
	if (VoxelMaterial != newMaterial)
	{
		VoxelMaterial = newMaterial;

		if (VoxelMaterial)
			VoxelMaterialDynamic = CreateAndSetMaterialInstanceDynamicFromMaterial(0, VoxelMaterial);
		else
			VoxelMaterialDynamic = CreateAndSetMaterialInstanceDynamicFromMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));

		MarkRenderStateDirty();

		if (SceneProxy)
		{
			((FEvercoastVoxelSceneProxy*)SceneProxy)->ResetMaterial(VoxelMaterialDynamic);
		}
	}
}

#if WITH_EDITOR
void UEvercoastVoxelRendererComp::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	SceneProxy = nullptr;
	for (int i = 0; i < 2; ++i)
	{
		if (CaptureWorldNormalRenderTarget[i])
		{
			CaptureWorldNormalRenderTarget[i]->ReleaseResource();
			CaptureWorldNormalRenderTarget[i] = nullptr;
		}
		if (FilteredWorldNormalRenderTarget[i])
		{
			FilteredWorldNormalRenderTarget[i]->ReleaseResource();
			FilteredWorldNormalRenderTarget[i] = nullptr;
		}
	}


	MarkRenderStateDirty(); // recreate sceneproxy by end of frame


	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UEvercoastVoxelRendererComp::SetUseIcosahedronForNormalGeneration(bool useIcosahedron)
{
	bUseIcosahedronForNormalGeneration = useIcosahedron;

	SceneProxy = nullptr;
	MarkRenderStateDirty();
}

void UEvercoastVoxelRendererComp::SetNormalVoxelSizeFactor(float size)
{
	NormalVoxelSizeFactor = size;

	SceneProxy = nullptr;
	MarkRenderStateDirty();
}

void UEvercoastVoxelRendererComp::SetGenerateWorldNormalSize(int32 size)
{
	GenerateWorldNormalSize = size;

	m_dirtyMark = true;

	// force regenerate sceneproxy
	SceneProxy = nullptr;
	for (int i = 0; i < 2; ++i)
	{
		if (CaptureWorldNormalRenderTarget[i])
		{
			CaptureWorldNormalRenderTarget[i]->ReleaseResource();
			CaptureWorldNormalRenderTarget[i] = nullptr;
		}
	}

	MarkRenderStateDirty();
}


void UEvercoastVoxelRendererComp::SetFilteredWorldNormalSize(int32 size)
{
	FilteredWorldNormalSize = size;

	m_dirtyMark = true;

	// force regenerate sceneproxy
	SceneProxy = nullptr;
	for (int i = 0; i < 2; ++i)
	{
		if (FilteredWorldNormalRenderTarget[i])
		{
			FilteredWorldNormalRenderTarget[i]->ReleaseResource();
			FilteredWorldNormalRenderTarget[i] = nullptr;
		}
	}

	MarkRenderStateDirty();
}