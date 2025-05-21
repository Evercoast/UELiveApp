#include "Gaussian/EvercoastGaussianSplatSceneProxy.h"
#include "Gaussian/EvercoastGaussianSplatPassthroughResult.h"
#include "Gaussian/EvercoastGaussianSplatComputeComponent.h"

FEvercoastGaussianSplatSceneProxy::FEvercoastGaussianSplatSceneProxy(const UEvercoastGaussianSplatComputeComponent* component, UMaterialInterface* material) :
	FPrimitiveSceneProxy(component),
	m_vertexFactory(GetScene().GetFeatureLevel(), "GaussianSplatOrientedQuadVertexFactory"),
	m_material(material),
	MaterialRelevance(component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
{
	InitialiseQuadMesh();

#if RHI_RAYTRACING
	if (IsRayTracingEnabled())
	{
		ENQUEUE_RENDER_COMMAND(InitProceduralMeshRayTracingGeometry)(
			[this, DebugName = component->GetFName()](FRHICommandListImmediate& RHICmdList)
		{
			FRayTracingGeometryInitializer Initializer;
			Initializer.DebugName = DebugName;
			Initializer.IndexBuffer = m_quadIndexBuffer.IndexBufferRHI;
			Initializer.TotalPrimitiveCount = m_quadIndexBuffer.Indices.Num() - 2; // triangle strips
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
			Segment.VertexBuffer = m_quadVertexBuffers.PositionVertexBuffer.VertexBufferRHI;
			Segment.NumPrimitives = RayTracingGeometry.Initializer.TotalPrimitiveCount;
#if ENGINE_MAJOR_VERSION == 5
			Segment.MaxVertices = m_quadVertexBuffers.PositionVertexBuffer.GetNumVertices();
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

FEvercoastGaussianSplatSceneProxy::~FEvercoastGaussianSplatSceneProxy()
{
	if (m_vertexFactory.IsInitialized())
		m_vertexFactory.ReleaseResource();

	if (m_quadVertexBuffers.StaticMeshVertexBuffer.IsInitialized())
		m_quadVertexBuffers.StaticMeshVertexBuffer.ReleaseResource();

	if (m_quadVertexBuffers.PositionVertexBuffer.IsInitialized())
		m_quadVertexBuffers.PositionVertexBuffer.ReleaseResource();

	if (m_quadVertexBuffers.ColorVertexBuffer.IsInitialized())
		m_quadVertexBuffers.ColorVertexBuffer.ReleaseResource();

	if (m_quadIndexBuffer.IsInitialized())
		m_quadIndexBuffer.ReleaseResource();

#if RHI_RAYTRACING
	if (IsRayTracingEnabled())
	{
		RayTracingGeometry.ReleaseResource();
	}
#endif

}

SIZE_T FEvercoastGaussianSplatSceneProxy::GetTypeHash() const
{
	// Seems like a best-practice thing for SceneProxy subclasses
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}


void FEvercoastGaussianSplatSceneProxy::InitialiseQuadMesh()
{
	TArray<FDynamicMeshVertex> OutVerts;

	// Calculate verts for a face pointing down Z
	FVector3f Positions[4] =
	{
		FVector3f(-1.0, 0, -1.0),
		FVector3f(+1.0, 0, -1.0),
		FVector3f(-1.0, 0, +1.0),
		FVector3f(+1.0, 0, +1.0)
	};
	FVector2D UV0s[4] =
	{
		FVector2D(0,1),
		FVector2D(1,1),
		FVector2D(0,0),
		FVector2D(1,0),
	};

	/*
	FVector2D UV1s[4] =
	{
		FVector2D(0,0),
		FVector2D(0,0),
		FVector2D(0,1),
		FVector2D(0,0),
	};
	*/

	// FDynamicMeshVertex should have 8 texcoord slots and automatically filled with UVs
	FDynamicMeshVertex Vertices[4];
	for (int32 VertexIndex = 0; VertexIndex < 4; VertexIndex++)
	{
		
		Vertices[VertexIndex] = FDynamicMeshVertex(
			Positions[VertexIndex] * 2.0f,
			FVector3f(1, 0, 0), // tangent x
			FVector3f(0, 0, 1), // tangent z
			FVector2f(UV0s[VertexIndex].X, UV0s[VertexIndex].Y),
			FColor::White
		);

		// DEBUG: modify the 2nd texcoord, otherwise two sets of texcoord will be the same
		//Vertices[VertexIndex].TextureCoordinate[1] = FVector2f(UV1s[VertexIndex].X, UV1s[VertexIndex].Y);

		OutVerts.Add(Vertices[VertexIndex]);

		

		m_quadIndexBuffer.Indices.Add(VertexIndex);
	}

	
	// Specify 4 sets of texcoord, 2 for conic data, 2 for mean position in world space
	m_quadVertexBuffers.InitFromDynamicVertex(&m_vertexFactory, OutVerts, 4);

	BeginInitResource(&m_quadIndexBuffer);
}


FPrimitiveViewRelevance FEvercoastGaussianSplatSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	// For splat rendering
	FPrimitiveViewRelevance Result;
	Result.bOpaque = false;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bRenderInDepthPass = false;
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bDrawRelevance = IsShown(View);
	Result.bStaticRelevance = false;
	Result.bDynamicRelevance = true;
	Result.bShadowRelevance = false;
	Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
	Result.bVelocityRelevance = false;

	MaterialRelevance.SetPrimitiveViewRelevance(Result); // CRITICAL: translucency relevance from Material settings
	return Result;
}

const FViewMatrices& FEvercoastGaussianSplatSceneProxy::ExtractRelevantViewMatrices(const FSceneView* pView) const
{
	return pView->ViewMatrices;
}
/*
void FEvercoastGaussianSplatSceneProxy::SaveEssentialReconData(const FMatrix& ObjectToWorld, const FMatrix& ViewProj, const FMatrix& InView, const FMatrix& InProj, const FVector4& InScreenParam, const FMatrix& InClipToWorld, bool isShadowPass) const
{
	SavedObjectToWorld = ObjectToWorld;
	SavedViewProj = ViewProj;
	SavedView = InView;
	SavedProj = InProj;
	SavedScreenParam = InScreenParam;
	SavedClipToWorld = InClipToWorld;
	SavedIsShadowPass = isShadowPass;
}

void FEvercoastGaussianSplatSceneProxy::PerformLateComputeShaderSplatRecon()
{
	m_vertexFactory.PerformComputeShaderSplatDataRecon(
		SavedObjectToWorld, FVector(),
		SavedViewProj, SavedView, SavedProj, SavedScreenParam, SavedClipToWorld, SavedIsShadowPass);

}
*/

void FEvercoastGaussianSplatSceneProxy::GetDynamicMeshElements(
	const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
	uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	std::lock_guard<std::recursive_mutex> guard(m_gaussianFrameLock);
	if (!m_encodedGaussian || !m_material)
		return;

	const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

	FMatrix EffectiveLocalToWorld;
	EffectiveLocalToWorld = GetLocalToWorld();

	auto MaterialRenderProxy = m_material->GetRenderProxy();
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
	FRHICommandListBase& RHICmdList = Collector.GetRHICommandList();
#endif
	const FMatrix& ObjectToWorld = this->GetLocalToWorld();
	//FMatrix ObjectToWorld = BaseComponent->GetComponentTransform().ToMatrixWithScale();
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

			// TODO: Need to do more for per eye rendering?
			
			const FViewMatrices& ViewMatrices = ExtractRelevantViewMatrices(pView);
			// TODO: pre-view translation???
			FVector PreViewTranslation = ViewMatrices.GetPreViewTranslation(); // need to apply after local to world transform???
			FMatrix ProjectionMatrix = ViewMatrices.GetProjectionMatrix();
			
			FMatrix ViewProjMatrix = ViewMatrices.GetViewProjectionMatrix(); //ViewMatrix * ProjectionMatrix;

			FMatrix ViewMatrix = ViewMatrices.GetViewMatrix();
			FMatrix ProjMatrix = ViewMatrices.GetProjectionMatrix();

			FMatrix ClipToWorld = ViewMatrices.GetInvViewProjectionMatrix();


			FVector4 ScreenParams = FVector4(pView->UnscaledViewRect.Width(), pView->UnscaledViewRect.Height(), 1.0 / pView->UnscaledViewRect.Width(), 1.0 / pView->UnscaledViewRect.Height());
			// Perform compute shader recon and transition for SRV use *before* sending mesh + vertex factory to callback
			m_vertexFactory.PerformComputeShaderSplatDataRecon(ObjectToWorld, PreViewTranslation, ViewProjMatrix, ViewMatrix, ProjMatrix, ScreenParams, ClipToWorld, bIsRenderingShadow);
			/*
			if (bPerformLateComputeShaderSplatRecon)
			{
				SaveEssentialReconData(ObjectToWorld, ViewProjMatrix, ViewMatrix, ProjMatrix, ScreenParams, ClipToWorld, bIsRenderingShadow);
			}
			else
			{
				m_vertexFactory.PerformComputeShaderSplatDataRecon(ObjectToWorld, PreViewTranslation, ViewProjMatrix, ViewMatrix, ProjMatrix, ScreenParams, ClipToWorld, bIsRenderingShadow);
			}
			*/

			FMeshBatch& Mesh = Collector.AllocateMesh();
			FMeshBatchElement& BatchElement = Mesh.Elements[0];
			BatchElement.IndexBuffer = &m_quadIndexBuffer;	// assign index buffer

			Mesh.bWireframe = bWireframe;
			Mesh.CastShadow = true;
			Mesh.bUseForDepthPass = true;
			Mesh.VertexFactory = &m_vertexFactory; // assign vertex factory
			Mesh.MaterialRenderProxy = MaterialRenderProxy; // assign material proxy

			bool bHasPrecomputedVolumetricLightmap;
			FMatrix PreviousLocalToWorld;
			int32 SingleCaptureIndex;
			bool bOutputVelocity;
			GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);

			FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
			DynamicPrimitiveUniformBuffer.Set(RHICmdList, GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, bOutputVelocity);
#elif ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
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
			BatchElement.NumPrimitives = m_quadIndexBuffer.Indices.Num() - 2;
			BatchElement.BaseVertexIndex = 0;
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = m_quadVertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;
			BatchElement.NumInstances = m_encodedGaussian->pointCount; // splat count
			/*
			BatchElement.DynamicPrimitiveData = nullptr;
			BatchElement.DynamicPrimitiveIndex = 0;
			BatchElement.DynamicPrimitiveInstanceSceneDataOffset = 0;
			*/
			Mesh.ReverseCulling = !IsLocalToWorldDeterminantNegative();
			Mesh.Type = PT_TriangleStrip;
			Mesh.DepthPriorityGroup = SDPG_World;
			Mesh.bCanApplyViewModeOverrides = false;

			Collector.AddMesh(ViewIndex, Mesh);
		}
	}
}


#if RHI_RAYTRACING
void FEvercoastGaussianSplatSceneProxy::GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances)
{
	if (!m_material)
		return;

	FMaterialRenderProxy* MaterialProxy = m_material->GetRenderProxy();

	if (RayTracingGeometry.RayTracingGeometryRHI.IsValid())
	{
		check(RayTracingGeometry.Initializer.IndexBuffer.IsValid());

		FRayTracingInstance RayTracingInstance;
		RayTracingInstance.Geometry = &RayTracingGeometry;
		RayTracingInstance.InstanceTransforms.Add(GetLocalToWorld());

		uint32 SectionIdx = 0;
		FMeshBatch MeshBatch;

		MeshBatch.VertexFactory = &m_vertexFactory;
		MeshBatch.SegmentIndex = 0;
		MeshBatch.MaterialRenderProxy = MaterialProxy;
		MeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
		MeshBatch.Type = PT_TriangleList;//PT_TriangleStrip;
		MeshBatch.DepthPriorityGroup = SDPG_World;
		MeshBatch.bCanApplyViewModeOverrides = false;
		MeshBatch.CastRayTracedShadow = IsShadowCast(Context.ReferenceView);

		FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
		BatchElement.IndexBuffer = &m_quadIndexBuffer;

		bool bHasPrecomputedVolumetricLightmap;
		FMatrix PreviousLocalToWorld;
		int32 SingleCaptureIndex;
		bool bOutputVelocity;
		GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);

		FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Context.RayTracingMeshResourceCollector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 4
		FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();
		DynamicPrimitiveUniformBuffer.Set(RHICmdList, GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, bOutputVelocity, GetCustomPrimitiveData());
#elif ENGINE_MINOR_VERSION >= 1
		DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, bOutputVelocity, GetCustomPrimitiveData());
#else
		DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, DrawsVelocity(), bOutputVelocity, GetCustomPrimitiveData());
#endif
#else
		DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, DrawsVelocity(), bOutputVelocity);
#endif
		BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = m_quadIndexBuffer.Indices.Num() / 3;//m_quadIndexBuffer.Indices.Num() - 2;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = m_quadVertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;

		RayTracingInstance.Materials.Add(MeshBatch);

#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 4
		// do nothing
#elif ENGINE_MINOR_VERSION >= 2
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


uint32 FEvercoastGaussianSplatSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize();
}

uint32 FEvercoastGaussianSplatSceneProxy::GetAllocatedSize() const
{
	return FPrimitiveSceneProxy::GetAllocatedSize() + (m_encodedGaussian ? m_encodedGaussian->GetSizeInBytes() : 0);
}


void FEvercoastGaussianSplatSceneProxy::SetEncodedGaussianSplat_RenderThread(FRHICommandListBase& RHICmdList, std::shared_ptr<const EvercoastGaussianSplatPassthroughResult> data)
{
	check(IsInRenderingThread());

	std::lock_guard<std::recursive_mutex> guard(m_gaussianFrameLock);

	m_encodedGaussian = data;
	m_vertexFactory.SetEncodedGaussianSplatData(m_encodedGaussian);
}


void FEvercoastGaussianSplatSceneProxy::LockGaussianData()
{
	m_gaussianFrameLock.lock();
}

void FEvercoastGaussianSplatSceneProxy::UnlockGaussianData()
{
	m_gaussianFrameLock.unlock();
}


static FBoxSphereBounds defaultBounds(FBox(FVector(-200, -200, -200), FVector(200, 200, 200)));
FBoxSphereBounds FEvercoastGaussianSplatSceneProxy::GetDefaultLocalBounds()
{
	return defaultBounds;
}

FBoxSphereBounds FEvercoastGaussianSplatSceneProxy::GetLocalBounds() const
{
	// TODO: return the read back GPU data
	return defaultBounds;
}


void FEvercoastGaussianSplatSceneProxy::ResetMaterial(UMaterialInterface* material)
{
	m_material = material;
}