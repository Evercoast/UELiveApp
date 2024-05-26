/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-13 01:51:05
* @Last Modified by:   Ye Feng
* @Last Modified time: 2021-11-17 05:48:45
*/


#include "EvercoastMVFVoxelRendererComp.h"
#include "EvercoastLocalVoxelFrame.h"
#include "ECV/VoxelSceneProxy.h"
#include "EvercoastBasicStreamingDataUploader.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// UEvercoastVoxelRendererComp
///////////////////////////////////////////////////////////////////////////////////////////////////////////

UEvercoastMVFVoxelRendererComp::UEvercoastMVFVoxelRendererComp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	/*
	bHiddenInGame = false;
	bUseEditorCompositing = true;
	SetGenerateOverlapEvents(false);
	*/
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	// Load our material we use for rendering
	/*static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMaterial(TEXT("/PointCloud/DefaultPointCloudMaterial"));
	VoxelMaterial = DefaultMaterial.Object;
	if (VoxelMaterial == nullptr)
	{
		VoxelMaterial = GEngine->WireframeMaterial;
	}*/

	//PrimaryComponentTick.bCanEverTick = true;
	//bTickInEditor = true; // need this to tick and subsequently call MarkRenderTransformDirty()
	//bUseAttachParentBound = false;
	m_dirtyMark = true;
	m_voxelUploader = std::make_shared<EvercoastBasicStreamingDataUploader>(this);
}

std::shared_ptr<IEvercoastStreamingDataUploader> UEvercoastMVFVoxelRendererComp::GetVoxelDataUploader() const
{
	return m_voxelUploader;
}

void UEvercoastMVFVoxelRendererComp::MarkDirty()
{
	m_dirtyMark = true;
}

/// <summary>
/// UEvercoastMVFVoxelRendererComp::CreateSceneProxy()
/// </summary>
/// <returns>New scene proxy for Evercoast voxel content</returns>

FPrimitiveSceneProxy* UEvercoastMVFVoxelRendererComp::CreateSceneProxy()
{
	if (!SceneProxy)
	{
		// whenever creates a new scene proxy, we mark both the voxel data and transform/bounds data dirty so that they gets updated in Tick()
		return new FVoxelSceneProxy(this, m_currLocalVoxelFrame);
	}

	return SceneProxy;
}

void UEvercoastMVFVoxelRendererComp::GetUsedMaterials(TArray< UMaterialInterface* >& OutMaterials, bool bGetDebugMaterials) const
{
	if (VoxelMaterial != nullptr)
	{
		OutMaterials.Add(VoxelMaterial);
	}
}

/// <summary>
/// Calculate bounding box/sphere for inherited USceneComponent
/// </summary>
/// <param name="LocalToWorld"></param>
/// <returns>A bounding structure of the content</returns>
FBoxSphereBounds UEvercoastMVFVoxelRendererComp::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds bounds = CalcLocalBounds();

	return bounds.TransformBy(LocalToWorld);
}

FBoxSphereBounds UEvercoastMVFVoxelRendererComp::CalcLocalBounds() const
{
	if (m_currLocalVoxelFrame)
	{
		return m_currLocalVoxelFrame->CalcBounds();
	}

	return FVoxelSceneProxy::GetDefaultVoxelDataBounds();
}

void UEvercoastMVFVoxelRendererComp::SetVoxelData(std::shared_ptr<EvercoastLocalVoxelFrame> localVoxelFrame)
{
	m_currLocalVoxelFrame = localVoxelFrame;

	std::shared_ptr<EvercoastLocalVoxelFrame> voxelFrame = localVoxelFrame;
	FVoxelSceneProxy* sceneProxy = (FVoxelSceneProxy*)(this->SceneProxy);
	
	ENQUEUE_RENDER_COMMAND(FEvercoastVoxelDataUpdate)(
		[sceneProxy, voxelFrame](FRHICommandListImmediate& RHICmdList)
		{
			sceneProxy->SetVoxelData_RenderThread(RHICmdList, voxelFrame);
		});

	m_dirtyMark = true;
}


void UEvercoastMVFVoxelRendererComp::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
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

void UEvercoastMVFVoxelRendererComp::SetVoxelMaterial(UMaterialInterface* newMaterial)
{
	if (VoxelMaterial != newMaterial)
	{
		VoxelMaterial = newMaterial;
		MarkRenderStateDirty();
	}
}

#if WITH_EDITOR
void UEvercoastMVFVoxelRendererComp::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// TODO: responde to properties that affects voxel rendering only
	MarkRenderStateDirty(); // recreate sceneproxy by end of frame

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UEvercoastMVFVoxelRendererComp::BeginDestroy()
{
	m_voxelUploader->ReleaseLocalResource();

	Super::BeginDestroy();
}