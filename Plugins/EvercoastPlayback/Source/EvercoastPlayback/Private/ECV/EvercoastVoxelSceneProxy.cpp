/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-13 01:51:05
* @Last Modified by:   Ye Feng
* @Last Modified time: 2021-11-17 05:47:40
*/
#include "EvercoastVoxelSceneProxy.h"
#include "EvercoastDecoder.h"
#include "EvercoastLocalVoxelFrame.h"
#include "PositionOnlyMesh.h"
#include "Filter/FilteringTarget.h"
#include "Filter/NormalGenerationShaders.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CortoMeshRendererComp.h" // for UE_LOG

FEvercoastVoxelSceneProxy::FEvercoastVoxelSceneProxy(UEvercoastVoxelRendererComp* component, UMaterialInstanceDynamic* material, bool generateNormal,
	bool useIcosahedron, float sizeFactor, int32 smoothIteration, UTextureRenderTarget2D* captureRenderTarget_L, UTextureRenderTarget2D* captureRenderTarget_R,
	UTextureRenderTarget2D* outputNormalTarget_L, UTextureRenderTarget2D* outputNormalTarget_R) :
	FPrimitiveSceneProxy(component)
	, VertexFactory_CubeMesh(GetScene().GetFeatureLevel(), "FEvercoastVoxelSceneProxy_CubeMesh")
	, VertexFactory_IcosahedronMesh(GetScene().GetFeatureLevel(), "FEvercoastVoxelSceneProxy_NormalMesh")
	, Material(material)
	, bNormalRender(generateNormal)
	, bUseIcosahedronForNormalRender(useIcosahedron)
	, MeshSizeFactor(sizeFactor)
	, NormalRender_SmoothIteration(smoothIteration)
	, CaptureRenderTarget{ captureRenderTarget_L, captureRenderTarget_R }
	, FilteredNormalRenderTarget{ outputNormalTarget_L, outputNormalTarget_R }
	, BaseComponent(component)
{
	InitialiseCubeMesh();

	if (bNormalRender)
	{
		// We'll only use left eye render target for scratchpad initialisation
		check(CaptureRenderTarget[0].Get() && FilteredNormalRenderTarget[0].Get());

		if (bUseIcosahedronForNormalRender)
			InitialiseIcosahedronMesh();

		NormalRender_DepthTarget = MakeShared<FGenericDepthTarget>();
		NormalRender_FilterTarget = MakeShared<FFlipFilterRenderTarget>();

		// Data initialisation
		NormalRender_DepthTarget->Init(CaptureRenderTarget[0]->GetSurfaceWidth(), CaptureRenderTarget[0]->GetSurfaceHeight()); // should match the render target color buffer
		NormalRender_FilterTarget->Init(FilteredNormalRenderTarget[0]->GetSurfaceWidth(), FilteredNormalRenderTarget[0]->GetSurfaceHeight(), FilteredNormalRenderTarget[0]->GetFormat()); // since it's a texture copy operation, the dimension has to be the same

		BeginInitResource(NormalRender_DepthTarget.Get());
		BeginInitResource(NormalRender_FilterTarget.Get());

		if (Material)
		{
			Material->SetTextureParameterValue(FName(TEXT("FilteredWorldNormalTex_L")), FilteredNormalRenderTarget[0].Get());
			Material->SetTextureParameterValue(FName(TEXT("FilteredWorldNormalTex_R")), FilteredNormalRenderTarget[1].Get());
		}

	}

	VertexFactory_CubeMesh.SetSceneProxy(this);

	if (bUseIcosahedronForNormalRender)
		VertexFactory_IcosahedronMesh.SetSceneProxy(this);

#if RHI_RAYTRACING
	if (IsRayTracingEnabled())
	{
		ENQUEUE_RENDER_COMMAND(InitProceduralMeshRayTracingGeometry)(
			[this, DebugName = component->GetFName()](FRHICommandListImmediate& RHICmdList)
		{
			FRayTracingGeometryInitializer Initializer;
			Initializer.DebugName = DebugName;
			Initializer.IndexBuffer = IndexBuffer_Cube.IndexBufferRHI;
			Initializer.TotalPrimitiveCount = IndexBuffer_Cube.Indices.Num() / 3;
			Initializer.GeometryType = RTGT_Triangles;
			Initializer.bFastBuild = true;
			Initializer.bAllowUpdate = false;

			RayTracingGeometry.SetInitializer(Initializer);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >=3 
			RayTracingGeometry.InitResource(RHICmdList);
#else
			RayTracingGeometry.InitResource();
#endif

			FRayTracingGeometrySegment Segment;
			Segment.VertexBuffer = VertexBuffers_Cube.PositionVertexBuffer.VertexBufferRHI;
			Segment.NumPrimitives = RayTracingGeometry.Initializer.TotalPrimitiveCount;
#if ENGINE_MAJOR_VERSION == 5
			Segment.MaxVertices = VertexBuffers_Cube.PositionVertexBuffer.GetNumVertices();
#endif
			RayTracingGeometry.Initializer.Segments.Add(Segment);

			//#dxr_todo: add support for segments?
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >=3 
			RayTracingGeometry.UpdateRHI(RHICmdList);
#else
			RayTracingGeometry.UpdateRHI();
#endif
		});
	}
#endif
}

SIZE_T FEvercoastVoxelSceneProxy::GetTypeHash() const
{
	// Seems like a best-practice thing for SceneProxy subclasses
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

FEvercoastVoxelSceneProxy::~FEvercoastVoxelSceneProxy()
{
	if (bNormalRender)
	{
		NormalRender_DepthTarget->ReleaseResource();
		NormalRender_FilterTarget->ReleaseResource();
	}

	if (VertexFactory_CubeMesh.IsInitialized())
		VertexFactory_CubeMesh.ReleaseResource();

	if (VertexFactory_IcosahedronMesh.IsInitialized())
		VertexFactory_IcosahedronMesh.ReleaseResource();

	if (VertexBuffers_Cube.StaticMeshVertexBuffer.IsInitialized())
		VertexBuffers_Cube.StaticMeshVertexBuffer.ReleaseResource();

	if (VertexBuffers_Cube.PositionVertexBuffer.IsInitialized())
		VertexBuffers_Cube.PositionVertexBuffer.ReleaseResource();

	if (VertexBuffers_Cube.ColorVertexBuffer.IsInitialized())
		VertexBuffers_Cube.ColorVertexBuffer.ReleaseResource();

	if (VertexBuffers_Icosahedron.StaticMeshVertexBuffer.IsInitialized())
		VertexBuffers_Icosahedron.StaticMeshVertexBuffer.ReleaseResource();

	if (VertexBuffers_Icosahedron.PositionVertexBuffer.IsInitialized())
		VertexBuffers_Icosahedron.PositionVertexBuffer.ReleaseResource();

	if (VertexBuffers_Icosahedron.ColorVertexBuffer.IsInitialized())
		VertexBuffers_Icosahedron.ColorVertexBuffer.ReleaseResource();

	if (IndexBuffer_Cube.IsInitialized())
		IndexBuffer_Cube.ReleaseResource();

	if (IndexBuffer_Icosahedron.IsInitialized())
		IndexBuffer_Icosahedron.ReleaseResource();

#if RHI_RAYTRACING
	if (IsRayTracingEnabled())
	{
		RayTracingGeometry.ReleaseResource();
	}
#endif

}

void FEvercoastVoxelSceneProxy::InitialiseCubeMesh()
{
	// Make a cube
	TArray<FDynamicMeshVertex> OutVerts;

	// Modified from: PrimitiveDrawingUtils.cpp, DrawBoxMesh()
	// Calculate verts for a face pointing down Z
	FVector Positions[4] =
	{
		FVector(-.50, -.50, +.50),
		FVector(-.50, +.50, +.50),
		FVector(+.50, +.50, +.50),
		FVector(+.50, -.50, +.50)
	};
	FVector2D UVs[4] =
	{
		FVector2D(0,0),
		FVector2D(0,1),
		FVector2D(1,1),
		FVector2D(1,0),
	};

	// Then rotate this face 6 times
	FRotator FaceRotations[6];
	FaceRotations[0] = FRotator(0, 0, 0);
	FaceRotations[1] = FRotator(90.f, 0, 0);
	FaceRotations[2] = FRotator(-90.f, 0, 0);
	FaceRotations[3] = FRotator(0, 0, 90.f);
	FaceRotations[4] = FRotator(0, 0, -90.f);
	FaceRotations[5] = FRotator(180.f, 0, 0);

	for (int32 f = 0; f < 6; f++)
	{
		FMatrix FaceTransform = FRotationMatrix(FaceRotations[f]);

		FDynamicMeshVertex Vertices[4];
		for (int32 VertexIndex = 0; VertexIndex < 4; VertexIndex++)
		{
			auto pos = FaceTransform.TransformPosition(Positions[VertexIndex]);
			FVector tx = FaceTransform.TransformVector(FVector(1, 0, 0));
			FVector tz = FaceTransform.TransformVector(FVector(0, 0, 1));
			Vertices[VertexIndex] = FDynamicMeshVertex(
				FVector3f(pos.X, pos.Y, pos.Z),
				FVector3f(tx.X, tx.Y, tx.Z), // tangent x
				FVector3f(tz.X, tz.Y, tz.Z), // tangent z
				FVector2f(UVs[VertexIndex].X, UVs[VertexIndex].Y),
				FColor::White
				);

			OutVerts.Add(Vertices[VertexIndex]);
		}

		IndexBuffer_Cube.Indices.Add(f * 4 + 0);
		IndexBuffer_Cube.Indices.Add(f * 4 + 1);
		IndexBuffer_Cube.Indices.Add(f * 4 + 2);
		IndexBuffer_Cube.Indices.Add(f * 4 + 0);
		IndexBuffer_Cube.Indices.Add(f * 4 + 2);
		IndexBuffer_Cube.Indices.Add(f * 4 + 3);
	}

	VertexBuffers_Cube.InitFromDynamicVertex(&VertexFactory_CubeMesh, OutVerts);

	// Enqueue initialization of render resource
	BeginInitResource(&IndexBuffer_Cube);
}

void FEvercoastVoxelSceneProxy::InitialiseIcosahedronMesh()
{
	const float PHI = 1.6180339887f;
	// Make a icosahedron
	TArray<FDynamicMeshVertex> OutVerts;

	FVector3f Vertices[12] = {
		FVector3f(-1, PHI, 0), 
		FVector3f(1, PHI, 0), 
		FVector3f(-1, -PHI, 0),
		FVector3f(1, -PHI, 0),

		FVector3f(0, -1, PHI),
		FVector3f(0, 1, PHI),
		FVector3f(0, -1, -PHI),
		FVector3f(0, 1, -PHI),

		FVector3f(PHI, 0, -1),
		FVector3f(PHI, 0, 1),
		FVector3f(-PHI, 0, -1),
		FVector3f(-PHI, 0, 1)
	};

	// Define the indices of the triangles that make up the icosahedron
	int32 Indices[60] = {
		0, 11, 5,
		0, 5, 1, 
		0, 1, 7, 
		0, 7, 10, 
		0, 10, 11,

		1, 5, 9, 
		5, 11, 4, 
		11, 10, 2, 
		10, 7, 6, 
		7, 1, 8,

		3, 9, 4, 
		3, 4, 2, 
		3, 2, 6, 
		3, 6, 8, 
		3, 8, 9,

		4, 9, 5, 
		2, 4, 11, 
		6, 2, 10, 
		8, 6, 7, 
		9, 8, 1
	};

	for (int32 VertexIndex = 0; VertexIndex < 12; VertexIndex++) {
		const auto& pos = Vertices[VertexIndex];
		FDynamicMeshVertex v = FDynamicMeshVertex(
			FVector3f(pos.X * MeshSizeFactor, pos.Y * MeshSizeFactor, pos.Z * MeshSizeFactor),
			FVector3f(1, 0, 0), // tangent x
			FVector3f(0, 0, 1), // tangent z
			FVector2f(0, 0),
			FColor::White
		);
		OutVerts.Add(v);
	}

	for (int32 i = 0; i < 60; i++) {
		IndexBuffer_Icosahedron.Indices.Add(Indices[i]);
	}

	VertexBuffers_Icosahedron.InitFromDynamicVertex(&VertexFactory_IcosahedronMesh, OutVerts);

	// Enqueue initialization of render resource
	BeginInitResource(&IndexBuffer_Icosahedron);
}


void FEvercoastVoxelSceneProxy::ResetMaterial(UMaterialInstanceDynamic* material)
{
	Material = material;
	if (bNormalRender)
	{
		if (Material)
		{
			Material->SetTextureParameterValue(FName(TEXT("FilteredWorldNormalTex_L")), FilteredNormalRenderTarget[0].Get());
			Material->SetTextureParameterValue(FName(TEXT("FilteredWorldNormalTex_R")), FilteredNormalRenderTarget[1].Get());
		}
	}

}

void FEvercoastVoxelSceneProxy::FilterWorldNormal_RenderThread(const FFlipFilterRenderTarget* FilterRenderTarget, const FTexture& SrcTexture, const FTexture& DstTexture, int FilterIteration, FRHICommandListImmediate& RHICmdList)
{
	IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>("Renderer");
	FilterRenderTarget->DoFilter(SrcTexture, DstTexture, FilterIteration, RHICmdList, RendererModule);
	// For debugging only
	//RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
}

void FEvercoastVoxelSceneProxy::RenderWorldNormal_RenderThread(const FTexture& DestTexture, const FGenericDepthTarget* NormalRender_DepthTarget,
	const FEvercoastInstancedCubeVertexFactory& vertexFactory, const FPositionVertexBuffer& PositionVertexBuffer, const FDynamicMeshIndexBuffer32& IndexBuffer,
	FMatrix ObjectToCamera, FMatrix CameraToWorld, FMatrix ObjectToProjection, std::shared_ptr<EvercoastLocalVoxelFrame> voxelFrame, FRHICommandListImmediate& RHICmdList)
{

	const FTexture2DRHIRef& DestinationTextureRHI = DestTexture.TextureRHI->GetTexture2D();
	const FTexture2DRHIRef& DestinationDepthTextureRHI = NormalRender_DepthTarget->DepthTextureRHI;

	// Need this for Vulkan
	RHICmdList.Transition(FRHITransitionInfo(DestinationTextureRHI, ERHIAccess::SRVMask, ERHIAccess::RTV));

	FRHIRenderPassInfo RPInfo(DestinationTextureRHI, ERenderTargetActions::Clear_Store, DestinationDepthTextureRHI, EDepthStencilTargetActions::ClearDepthStencil_StoreDepthStencil); // DontLoad_Store??

	RHICmdList.BeginRenderPass(RPInfo, TEXT("Custom Bake World Normal"));
	{
		RHICmdList.SetViewport(0, 0, 0.0f, DestinationTextureRHI->GetSizeX(), DestinationTextureRHI->GetSizeY(), 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();//TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI(); // TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI();
		GraphicsPSOInit.DepthStencilTargetFormat = DestinationDepthTextureRHI->GetFormat();
		GraphicsPSOInit.DepthStencilAccess = FExclusiveDepthStencil::DepthWrite_StencilNop;
//		GraphicsPSOInit.bDepthBounds = true;

		// New engine version...
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FVoxelWorldNormalGenVS> VertexShader(ShaderMap);
		TShaderMapRef<FVoxelWorldNormalGenPS> PixelShader(ShaderMap);

		check(VertexShader.IsValid());
		check(PixelShader.IsValid());
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = vertexFactory.GetDeclaration(EVertexInputStreamType::PositionOnly);// NormalRender_VertexDeclaration->VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

#if ENGINE_MAJOR_VERSION >= 5
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
#else
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
#endif

		float positionRescaleFactor = 1.0f / ((1 << voxelFrame->m_bitsPerVoxel) - 1);

		VertexShader->SetTransforms(RHICmdList, ObjectToCamera, ObjectToProjection);
		VertexShader->SetInstancingData(RHICmdList, 
			voxelFrame->m_boundsMin,
			voxelFrame->m_boundsDim,
			positionRescaleFactor,
			100.0f,
			vertexFactory.GetPositionBufferSRV());

		PixelShader->SetTransforms(RHICmdList, CameraToWorld);

		// Get RHI vertex buffer from mesh
		RHICmdList.SetStreamSource(0, PositionVertexBuffer.VertexBufferRHI, 0);

		// Get RHI index buffer from mesh
		RHICmdList.DrawIndexedPrimitive(
			IndexBuffer.IndexBufferRHI,
			/*BaseVertexIndex=*/ 0,
			/*MinIndex=*/ 0,
			/*NumVertices=*/ PositionVertexBuffer.GetNumVertices(),
			/*StartIndex=*/ 0,
			/*NumPrimitives=*/ IndexBuffer.Indices.Num() / 3,
			/*NumInstances=*/ std::min(DECODER_MAX_VOXEL_COUNT, voxelFrame->m_voxelCount) // clamp max cube/voxel count
		);
	}
	RHICmdList.EndRenderPass();
	// For debugging
	//RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);

}

FPrimitiveViewRelevance FEvercoastVoxelSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bRenderInDepthPass = true;
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bDrawRelevance = IsShown(View);
	Result.bStaticRelevance = true;
	Result.bDynamicRelevance = true;
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
	Result.bVelocityRelevance = false;
	return Result;
}

void FEvercoastVoxelSceneProxy::GetDynamicMeshElements(
	const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, 
	uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	std::lock_guard<std::recursive_mutex> guard(m_voxelFrameLock);
	if (!m_voxelFrame)
		return;

	const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

	FMatrix EffectiveLocalToWorld;
	EffectiveLocalToWorld = GetLocalToWorld();

	auto MaterialRenderProxy = Material->GetRenderProxy();

	FMatrix ObjectToWorld = BaseComponent->GetComponentTransform().ToMatrixWithScale();
	// allocate mesh from collector
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* pView = Views[ViewIndex];
			// Calculate the view-dependent scaling factor.
			float ViewScale = 1.0f;

			bool isInstancedStereo = pView->IsInstancedStereoPass();
			bool isFullPass = pView->StereoPass == EStereoscopicPass::eSSP_FULL;
			//const FSceneView& StereoEyeView = ViewFamily.GetStereoEyeView(pView->StereoPass);

			// TODO: better way finding it's shadow pass or not
			const bool bIsRenderingShadow = pView->ShadowViewMatrices.GetViewMatrix() != pView->ViewMatrices.GetViewMatrix();

			if (bNormalRender && !bIsRenderingShadow)
			{
				FMatrix ProjectionMatrix = pView->ViewMatrices.GetProjectionMatrix();
				FMatrix ViewMatrix = pView->ViewMatrices.GetViewMatrix();

				FMatrix ObjectToCamera = ObjectToWorld * ViewMatrix;
				FMatrix CameraToWorld = ViewMatrix.Inverse();
				FMatrix ObjectToProjection = ObjectToWorld * ViewMatrix * ProjectionMatrix;

				int renderTargetIndex = -1;
				if (isFullPass || IStereoRendering::IsAPrimaryPass(pView->StereoPass))
				{
					renderTargetIndex = 0;
				}
				else if (IStereoRendering::IsASecondaryPass(pView->StereoPass))
				{
					renderTargetIndex = 1;
				}

				if (renderTargetIndex >= 0)
				{
					ENQUEUE_RENDER_COMMAND(CustomRenderWorldNormal)(
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
						[=, this](FRHICommandListImmediate& RHICmdList)
#else
						[=](FRHICommandListImmediate& RHICmdList)
#endif
						{
							if (!CaptureRenderTarget[renderTargetIndex].IsValid() || !FilteredNormalRenderTarget[renderTargetIndex].IsValid())
								return;

							if (bUseIcosahedronForNormalRender)
							{
								RenderWorldNormal_RenderThread(*CaptureRenderTarget[renderTargetIndex]->GetResource(), NormalRender_DepthTarget.Get(),
									VertexFactory_IcosahedronMesh, VertexBuffers_Icosahedron.PositionVertexBuffer, IndexBuffer_Icosahedron,
									ObjectToCamera, CameraToWorld, ObjectToProjection, m_voxelFrame, RHICmdList);
							}
							else
							{
								RenderWorldNormal_RenderThread(*CaptureRenderTarget[renderTargetIndex]->GetResource(), NormalRender_DepthTarget.Get(),
									VertexFactory_CubeMesh, VertexBuffers_Cube.PositionVertexBuffer, IndexBuffer_Cube,
									ObjectToCamera, CameraToWorld, ObjectToProjection, m_voxelFrame, RHICmdList);
							}

							// filter
							FilterWorldNormal_RenderThread(NormalRender_FilterTarget.Get(), *CaptureRenderTarget[renderTargetIndex]->GetResource(),
								*FilteredNormalRenderTarget[renderTargetIndex]->GetResource(), NormalRender_SmoothIteration, RHICmdList);
						}
					);
				}
				else
				{
					UE_LOG(EvercoastRendererLog, Warning, TEXT("Unhandled EStereoscopicPass: %d"), (int)pView->StereoPass);
				}
			}

			FMeshBatch& Mesh = Collector.AllocateMesh();
			FMeshBatchElement& BatchElement = Mesh.Elements[0];
			BatchElement.IndexBuffer = &IndexBuffer_Cube;	// assign index buffer

			Mesh.bWireframe = bWireframe;
			Mesh.CastShadow = true;
			Mesh.bUseForDepthPass = true;
			Mesh.VertexFactory = &VertexFactory_CubeMesh; // assign vertex factory
			Mesh.MaterialRenderProxy = MaterialRenderProxy; // assign material proxy

			bool bHasPrecomputedVolumetricLightmap;
			FMatrix PreviousLocalToWorld;
			int32 SingleCaptureIndex;
			bool bOutputVelocity;
			GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);

			FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
			DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, bOutputVelocity);
#else
			DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, DrawsVelocity(), bOutputVelocity);
#endif
			BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;
			// Setting PrimID_FromPrimitiveSceneInfo will requires PrimitiveIDStream in vertex factory, which in turn will forbide you from setting uniform buffer
			// Setting PrimID_DynamicPrimitiveShaderData will complain primitive not being processed, because there's no primitive-wise data declared/assigned(all stored in texture) so DynamicPrimitiveData will not be updated
			// The last resort and actually the reasonable choice is to disable primitive id then set uniform buffer, making UE think it doesn't want instancing
			// but actually we want to do it outside UE's framework, for performance and simplicity reason
			BatchElement.PrimitiveIdMode = PrimID_ForceZero;
			
			BatchElement.FirstIndex = 0;
			BatchElement.NumPrimitives = IndexBuffer_Cube.Indices.Num() / 3;
			BatchElement.BaseVertexIndex = 0;
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = VertexBuffers_Cube.PositionVertexBuffer.GetNumVertices() - 1;
			BatchElement.NumInstances = std::min(DECODER_MAX_VOXEL_COUNT, m_voxelFrame->m_voxelCount); // voxel count
			/*
			BatchElement.DynamicPrimitiveData = nullptr;
			BatchElement.DynamicPrimitiveIndex = 0;
			BatchElement.DynamicPrimitiveInstanceSceneDataOffset = 0;
			*/
			Mesh.ReverseCulling = !IsLocalToWorldDeterminantNegative();
			Mesh.Type = PT_TriangleList;
			Mesh.DepthPriorityGroup = SDPG_World;
			Mesh.bCanApplyViewModeOverrides = false;

			Collector.AddMesh(ViewIndex, Mesh);
		}
	}
}

#if RHI_RAYTRACING
void FEvercoastVoxelSceneProxy::GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances)
{
	if (!Material)
		return;

	FMaterialRenderProxy* MaterialProxy = Material->GetRenderProxy();

	if (RayTracingGeometry.RayTracingGeometryRHI.IsValid())
	{
		check(RayTracingGeometry.Initializer.IndexBuffer.IsValid());

		FRayTracingInstance RayTracingInstance;
		RayTracingInstance.Geometry = &RayTracingGeometry;
		RayTracingInstance.InstanceTransforms.Add(GetLocalToWorld());

		uint32 SectionIdx = 0;
		FMeshBatch MeshBatch;

		MeshBatch.VertexFactory = &VertexFactory_CubeMesh;
		MeshBatch.SegmentIndex = 0;
		MeshBatch.MaterialRenderProxy = Material->GetRenderProxy();
		MeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
		MeshBatch.Type = PT_TriangleList;
		MeshBatch.DepthPriorityGroup = SDPG_World;
		MeshBatch.bCanApplyViewModeOverrides = false;
		MeshBatch.CastRayTracedShadow = IsShadowCast(Context.ReferenceView);

		FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
		BatchElement.IndexBuffer = &IndexBuffer_Cube;

		bool bHasPrecomputedVolumetricLightmap;
		FMatrix PreviousLocalToWorld;
		int32 SingleCaptureIndex;
		bool bOutputVelocity;
		GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);

		FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Context.RayTracingMeshResourceCollector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 1
		DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, bOutputVelocity, GetCustomPrimitiveData());
#else
		DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, DrawsVelocity(), bOutputVelocity, GetCustomPrimitiveData());
#endif
#else
		DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, DrawsVelocity(), bOutputVelocity);
#endif
		BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = IndexBuffer_Cube.Indices.Num() / 3;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = VertexBuffers_Cube.PositionVertexBuffer.GetNumVertices() - 1;

		RayTracingInstance.Materials.Add(MeshBatch);

#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 2
		Context.BuildInstanceMaskAndFlags(RayTracingInstance, *this);
#else
		RayTracingInstance.BuildInstanceMaskAndFlags(GetScene().GetFeatureLevel());
#endif
#else
		RayTracingInstance.BuildInstanceMaskAndFlags();
#endif
		OutRayTracingInstances.Add(RayTracingInstance);
	}
}
#endif

uint32 FEvercoastVoxelSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize();
}

uint32 FEvercoastVoxelSceneProxy::GetAllocatedSize() const
{
	return FPrimitiveSceneProxy::GetAllocatedSize() + (m_voxelFrame ? m_voxelFrame->m_voxelDataSize : 0);
}

void FEvercoastVoxelSceneProxy::SetVoxelData_RenderThread(FRHICommandListBase& RHICmdList, std::shared_ptr<EvercoastLocalVoxelFrame> data)
{
	check(IsInRenderingThread());

	std::lock_guard<std::recursive_mutex> guard(m_voxelFrameLock);

	m_voxelFrame = data;
	VertexFactory_CubeMesh.SetInstancingData(m_voxelFrame);
	if (bNormalRender && bUseIcosahedronForNormalRender)
		VertexFactory_IcosahedronMesh.SetInstancingData(m_voxelFrame);
}

void FEvercoastVoxelSceneProxy::OnTransformChanged()
{

}

void FEvercoastVoxelSceneProxy::LockVoxelData()
{
	m_voxelFrameLock.lock();
}

void FEvercoastVoxelSceneProxy::UnlockVoxelData()
{
	m_voxelFrameLock.unlock();
}

static FBoxSphereBounds defaultBounds(FBox(FVector(-50, -50, -50), FVector(50, 50, 50)));

FBoxSphereBounds FEvercoastVoxelSceneProxy::GetVoxelDataBounds() const
{
	std::lock_guard<std::recursive_mutex> guard(m_voxelFrameLock);
	if (m_voxelFrame)
	{
		return m_voxelFrame->CalcBounds();
	}

	return defaultBounds;
}


FBoxSphereBounds FEvercoastVoxelSceneProxy::GetDefaultVoxelDataBounds()
{
	return defaultBounds;
}
