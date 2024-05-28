#include "CortoMeshRendererComp.h"
#include "Engine.h"
#include "CortoLocalMeshFrame.h"
#include "CortoDataUploader.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "MediaPlayerFacade.h"
#include "VertexFactory.h"
#include "Engine/TextureRenderTarget2D.h"
#include "PositionOnlyMesh.h"
#include "ScreenRendering.h"
#include "Filter/FilteringTarget.h"
#include "Filter/NormalGenerationShaders.h"
#include "PrimitiveSceneProxy.h"
#include "StaticMeshResources.h"
#include "Modules/ModuleManager.h"
#include "RendererInterface.h"
#include "MeshPassProcessor.h"
#include "MeshPassProcessor.inl"
#include "DynamicMeshBuilder.h"
#include "MaterialShared.h"
#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 2
#include "MaterialDomain.h"
#endif
#endif

#if RHI_RAYTRACING
#include "RayTracingDefinitions.h"
#include "RayTracingInstance.h"
#endif

DEFINE_LOG_CATEGORY(EvercoastRendererLog);

DEFINE_LOG_CATEGORY_STATIC(LogProceduralComponent, Log, All);

/** Procedural mesh scene proxy */
class FCortoMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	void ReinitialiseBuffers_RenderThread(int32 NewNumVerts, int32 NewNumIndices)
	{
		check(IsInRenderingThread());

		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();

		// Allocate index buffer
		IndexBuffer.Indices.SetNumZeroed(NewNumIndices);
		// Allocate various vertex buffers
		VertexBuffers.StaticMeshVertexBuffer.SetUseFullPrecisionUVs(true);
		VertexBuffers.InitWithDummyData(&VertexFactory, NewNumVerts, 1, 0);

		// Enqueue initialization of render resource
		BeginInitResource(&VertexBuffers.PositionVertexBuffer);
		BeginInitResource(&VertexBuffers.StaticMeshVertexBuffer);
		BeginInitResource(&VertexBuffers.ColorVertexBuffer);
		BeginInitResource(&IndexBuffer);

		// Normal map render
		if (bNormalRender)
		{
			NormalRender_PositionVertexBuffer->ReleaseResource();
			NormalRender_IndexBuffer->ReleaseResource();

			NormalRender_PositionVertexBuffer->Init(NewNumVerts);
			NormalRender_IndexBuffer->Indices.SetNumZeroed(NewNumIndices);

			BeginInitResource(NormalRender_PositionVertexBuffer.Get());
			BeginInitResource(NormalRender_IndexBuffer.Get());
		}


	}

	FCortoMeshSceneProxy(UCortoMeshRendererComp* Component, UMaterialInstanceDynamic* material, bool generateNormal, int32 smoothIteration, UTextureRenderTarget2D* captureRenderTarget_L, UTextureRenderTarget2D* captureRenderTarget_R, UTextureRenderTarget2D* outputNormalTarget_L, UTextureRenderTarget2D* outputNormalTarget_R)
		: FPrimitiveSceneProxy(Component)
		, NumVerts(0)
		, NumIndices(0)
		, Material(material)
		, VertexFactory(GetScene().GetFeatureLevel(), "FCortoMeshProxySection")
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
		, bNormalRender(generateNormal)
		, NormalRender_SmoothIteration(smoothIteration)
		, CaptureRenderTarget{ captureRenderTarget_L, captureRenderTarget_R}
		, FilteredNormalRenderTarget{ outputNormalTarget_L, outputNormalTarget_R}
		, BaseComponent(Component)
	{
		// Soft limit, will increase on demand
		const int32 MaxNumVerts = 100000;
		const int32 MaxNumIndices = 300000;

		// Allocate index buffer
		IndexBuffer.Indices.SetNumZeroed(MaxNumIndices);
		// Allocate various vertex buffers
		VertexBuffers.StaticMeshVertexBuffer.SetUseFullPrecisionUVs(true);
		VertexBuffers.InitWithDummyData(&VertexFactory, MaxNumVerts, 1, 0);

		// Enqueue initialization of render resource
		BeginInitResource(&VertexBuffers.PositionVertexBuffer);
		BeginInitResource(&VertexBuffers.StaticMeshVertexBuffer);
		BeginInitResource(&VertexBuffers.ColorVertexBuffer);
		BeginInitResource(&IndexBuffer);

		BeginInitResource(&VertexFactory);

		
		//check(Material);

		if (bNormalRender)
		{
			// We'll just have one normal render target as it's a scratchpad buffer for both left and right output
			check(CaptureRenderTarget[0].Get() && FilteredNormalRenderTarget[0].Get());

			NormalRender_VertexDeclaration = MakeShared<FPositionOnlyVertexDeclaration>();
			NormalRender_PositionVertexBuffer = MakeShared<FPositionOnlyVertexBuffer>();
			NormalRender_IndexBuffer = MakeShared<FDynamicMeshIndexBuffer32>();
			NormalRender_DepthTarget = MakeShared<FGenericDepthTarget>();
			NormalRender_FilterTarget = MakeShared<FFlipFilterRenderTarget>();

			// Data initialisation
			NormalRender_PositionVertexBuffer->Init(MaxNumVerts);
			NormalRender_IndexBuffer->Indices.SetNumZeroed(MaxNumIndices);
			NormalRender_DepthTarget->Init(CaptureRenderTarget[0]->GetSurfaceWidth(), CaptureRenderTarget[0]->GetSurfaceHeight()); // should match the render target color buffer
			NormalRender_FilterTarget->Init(FilteredNormalRenderTarget[0]->GetSurfaceWidth(), FilteredNormalRenderTarget[0]->GetSurfaceHeight(), FilteredNormalRenderTarget[0]->GetFormat()); // since it's a texture copy operation, the dimension has to be the same

			BeginInitResource(NormalRender_VertexDeclaration.Get());
			BeginInitResource(NormalRender_PositionVertexBuffer.Get());
			BeginInitResource(NormalRender_IndexBuffer.Get());
			BeginInitResource(NormalRender_DepthTarget.Get());
			BeginInitResource(NormalRender_FilterTarget.Get());

			if (Material)
			{
				Material->SetTextureParameterValue(FName(TEXT("FilteredWorldNormalTex_L")), FilteredNormalRenderTarget[0].Get());
				Material->SetTextureParameterValue(FName(TEXT("FilteredWorldNormalTex_R")), FilteredNormalRenderTarget[1].Get());
			}
		}

		

#if RHI_RAYTRACING
		if (IsRayTracingEnabled())
		{
			ENQUEUE_RENDER_COMMAND(InitProceduralMeshRayTracingGeometry)(
				[this, DebugName = Component->GetFName()](FRHICommandListImmediate& RHICmdList)
			{
				FRayTracingGeometryInitializer Initializer;
				Initializer.DebugName = DebugName;
				Initializer.IndexBuffer = nullptr;
				Initializer.TotalPrimitiveCount = 0;
				Initializer.GeometryType = RTGT_Triangles;
				Initializer.bFastBuild = true;
				Initializer.bAllowUpdate = false;

				RayTracingGeometry.SetInitializer(Initializer);
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
				RayTracingGeometry.InitResource(RHICmdList);
#else
				RayTracingGeometry.InitResource();
#endif

				RayTracingGeometry.Initializer.IndexBuffer = IndexBuffer.IndexBufferRHI;
				RayTracingGeometry.Initializer.TotalPrimitiveCount = IndexBuffer.Indices.Num() / 3;

				FRayTracingGeometrySegment Segment;
				Segment.VertexBuffer = VertexBuffers.PositionVertexBuffer.VertexBufferRHI;
				Segment.NumPrimitives = RayTracingGeometry.Initializer.TotalPrimitiveCount;
#if ENGINE_MAJOR_VERSION == 5
				Segment.MaxVertices = VertexBuffers.PositionVertexBuffer.GetNumVertices();
#endif
				RayTracingGeometry.Initializer.Segments.Add(Segment);

				//#dxr_todo: add support for segments?
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
				RayTracingGeometry.UpdateRHI(RHICmdList);
#else
				RayTracingGeometry.UpdateRHI();
#endif
			});
		}
#endif
	}

	virtual ~FCortoMeshSceneProxy()
	{
		// Since SceneProxies are destroyed on render threads, we call ReleaseResource() instead of BeginReleaseResource()
		if (bNormalRender)
		{
			NormalRender_VertexDeclaration->ReleaseResource();
			NormalRender_PositionVertexBuffer->ReleaseResource();
			NormalRender_IndexBuffer->ReleaseResource();
			NormalRender_DepthTarget->ReleaseResource();
			NormalRender_FilterTarget->ReleaseResource();
		}

		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();

#if RHI_RAYTRACING
		if (IsRayTracingEnabled())
		{
			RayTracingGeometry.ReleaseResource();
		}
#endif

	}

	void ResetMaterial(UMaterialInstanceDynamic* material)
	{
		Material = material;
		//check(Material);
		if (bNormalRender)
		{
			if (Material)
			{
				Material->SetTextureParameterValue(FName(TEXT("FilteredWorldNormalTex_L")), FilteredNormalRenderTarget[0].Get());
				Material->SetTextureParameterValue(FName(TEXT("FilteredWorldNormalTex_R")), FilteredNormalRenderTarget[1].Get());
			}
		}
		
	}

	void SetMeshData(std::shared_ptr<CortoLocalMeshFrame> localMeshFrame)
	{
		const auto& meshFrame = *localMeshFrame;
		int32_t newNumVerts = meshFrame.GetVertexCount();
		int32_t newNumIndices = meshFrame.GetIndexCount();
		int32_t newNumTriangles = meshFrame.m_triangleCount;

		// check vertex and index count
		if (newNumVerts > (int32)VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() ||
			newNumIndices > IndexBuffer.Indices.Num())
		{
			UE_LOG(EvercoastRendererLog, Warning, TEXT("Current mesh data exceeds soft limit, increase vertex and index buffer by 100%%. Requested vertices: %d Requested indices: %d"), newNumVerts, newNumIndices);

            auto promise = MakeShared<TPromise<void>, ESPMode::ThreadSafe>();
            auto future = promise->GetFuture();

            ENQUEUE_RENDER_COMMAND(FEvercoastMeshDataReinit)(
                [this, newNumVerts, newNumIndices, promise](FRHICommandListImmediate& RHICmdList)
                {
                    ReinitialiseBuffers_RenderThread(newNumVerts, newNumIndices);
                    promise->SetValue();
                });
			
            future.Get();
		}



		bool hasNormal = meshFrame.m_normalBuffer.Num() == newNumVerts;

		if (newNumIndices > 0)
		{
			int IndexTypeSize = IndexBuffer.Indices.GetTypeSize();
			FMemory::Memcpy(IndexBuffer.Indices.GetData(), meshFrame.m_indexBuffer.GetData(), newNumIndices * IndexTypeSize);
		}

		if (newNumVerts > 0)
		{
			int PositionTypeSize = VertexBuffers.PositionVertexBuffer.GetStride();
			FMemory::Memcpy(VertexBuffers.PositionVertexBuffer.GetVertexData(), meshFrame.m_positionBuffer.GetData(), newNumVerts * PositionTypeSize);

			int TexcoordTypeSize = VertexBuffers.StaticMeshVertexBuffer.GetTexCoordSize() / VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords() / VertexBuffers.StaticMeshVertexBuffer.GetNumVertices();
			FMemory::Memcpy(VertexBuffers.StaticMeshVertexBuffer.GetTexCoordData(), meshFrame.m_uvBuffer.GetData(), newNumVerts * TexcoordTypeSize);
		}

		// Iterate through vertex data, copying in new tangents if vertex data contains normal
		// Note this is different from bNormalRender
		if (hasNormal)
		{
			for (int32 i = 0; i < newNumVerts; i++)
			{
				const auto TangentX = FPackedNormal(FVector3f(1.f, 0.f, 0.f));
				FPackedNormal TangentZ = FPackedNormal(meshFrame.m_normalBuffer[i]);
				TangentZ.Vector.W = 127;

				const auto TangentY = FVector3f(GenerateYAxis(TangentX, TangentZ));	//LWC_TODO: Precision loss

#if ENGINE_MAJOR_VERSION == 5
				VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(i, TangentX.ToFVector3f(), TangentY, TangentZ.ToFVector3f());
#else
				VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(i, TangentX.ToFVector(), TangentY, TangentZ.ToFVector());
#endif
			}
		}

		if (bNormalRender)
		{
			// For world space normal rendering stuff
			// Copy verts and indices
			if (newNumVerts > 0)
			{
				const FVector3f* src = meshFrame.m_positionBuffer.GetData();
				NormalRender_PositionVertexBuffer->CopyVertices((FPositionOnlyVertex*)src, newNumVerts);
			}

			if (newNumIndices > 0)
			{
				check(NormalRender_IndexBuffer->Indices.GetTypeSize() == meshFrame.m_indexBuffer.GetTypeSize());
				FMemory::Memcpy(NormalRender_IndexBuffer->Indices.GetData(), meshFrame.m_indexBuffer.GetData(), newNumIndices * NormalRender_IndexBuffer->Indices.GetTypeSize());
			}
		}

		

		auto promise = MakeShared<TPromise<void>, ESPMode::ThreadSafe>();
		auto future = promise->GetFuture();

		ENQUEUE_RENDER_COMMAND(FEvercoastMeshDataUpdate)(
			[this, meshFrame=localMeshFrame, newNumVerts, newNumIndices, promise](FRHICommandListImmediate& RHICmdList)
			{
				UploadMeshData_RenderThread(meshFrame, newNumVerts, newNumIndices, RHICmdList);
				RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);

				promise->SetValue();
			});

		future.Get();

		// Update those numbers at last, as it will be used in parallel in GetDynamicMeshElements()
		NumVerts = newNumVerts;
		NumIndices = newNumIndices;
		NumTriangles = newNumTriangles;

	}

	/** Called on render thread to assign new dynamic data */
	void UploadMeshData_RenderThread(std::shared_ptr<CortoLocalMeshFrame> localMeshFrame, int32_t newNumVerts, int32_t newNumIndices, FRHICommandListImmediate& RHICmdList)
	{
		check(IsInRenderingThread());

		// linearize guaranteed by promise/future pair in SetMeshData()

		const auto& meshFrame = *localMeshFrame;

		int IndexTypeSize = IndexBuffer.Indices.GetTypeSize();
		int PositionTypeSize = VertexBuffers.PositionVertexBuffer.GetStride();
		int TexcoordTypeSize = VertexBuffers.StaticMeshVertexBuffer.GetTexCoordSize() / VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords() / VertexBuffers.StaticMeshVertexBuffer.GetNumVertices();

#if ENGINE_MAJOR_VERSION == 5
		if (newNumIndices > 0)
		{
			auto& srcIndexBuffer = IndexBuffer.Indices;
#if ENGINE_MINOR_VERSION >= 3
			void* dstIndexBuffer = RHICmdList.LockBuffer(IndexBuffer.IndexBufferRHI, 0, newNumIndices * IndexTypeSize, RLM_WriteOnly);
			FMemory::Memcpy(dstIndexBuffer, srcIndexBuffer.GetData(), newNumIndices * IndexTypeSize);
			RHICmdList.UnlockBuffer(IndexBuffer.IndexBufferRHI);
#else
			void* dstIndexBuffer = RHILockBuffer(IndexBuffer.IndexBufferRHI, 0, newNumIndices * IndexTypeSize, RLM_WriteOnly);
			FMemory::Memcpy(dstIndexBuffer, srcIndexBuffer.GetData(), newNumIndices * IndexTypeSize);
			RHIUnlockBuffer(IndexBuffer.IndexBufferRHI);
#endif
		}

		if (newNumVerts > 0)
		{
			auto& VertexBuffer = VertexBuffers.PositionVertexBuffer;
#if ENGINE_MINOR_VERSION >= 3
			void* VertexBufferData = RHICmdList.LockBuffer(VertexBuffer.VertexBufferRHI, 0, newNumVerts * PositionTypeSize, RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), newNumVerts * PositionTypeSize);
			RHICmdList.UnlockBuffer(VertexBuffer.VertexBufferRHI);
#else
			void* VertexBufferData = RHILockBuffer(VertexBuffer.VertexBufferRHI, 0, newNumVerts * PositionTypeSize, RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), newNumVerts * PositionTypeSize);
			RHIUnlockBuffer(VertexBuffer.VertexBufferRHI);
#endif
		}

		// Leave the color buffer and tangents untouched as we don't need them
		/*
		{
			auto& VertexBuffer = VertexBuffers.ColorVertexBuffer;
			void* VertexBufferData = RHILockBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
			RHIUnlockBuffer(VertexBuffer.VertexBufferRHI);
		}

		{
			auto& VertexBuffer = VertexBuffers.StaticMeshVertexBuffer;
			void* VertexBufferData = RHILockBuffer(VertexBuffer.TangentsVertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetTangentSize(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetTangentData(), VertexBuffer.GetTangentSize());
			RHIUnlockBuffer(VertexBuffer.TangentsVertexBuffer.VertexBufferRHI);
		}
		*/
		if (newNumVerts > 0)
		{
			auto& VertexBuffer = VertexBuffers.StaticMeshVertexBuffer;
#if ENGINE_MINOR_VERSION >= 3
			void* VertexBufferData = RHICmdList.LockBuffer(VertexBuffer.TexCoordVertexBuffer.VertexBufferRHI, 0, newNumVerts * TexcoordTypeSize, RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetTexCoordData(), newNumVerts * TexcoordTypeSize);
			RHICmdList.UnlockBuffer(VertexBuffer.TexCoordVertexBuffer.VertexBufferRHI);
#else
			void* VertexBufferData = RHILockBuffer(VertexBuffer.TexCoordVertexBuffer.VertexBufferRHI, 0, newNumVerts * TexcoordTypeSize, RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetTexCoordData(), newNumVerts * TexcoordTypeSize);
			RHIUnlockBuffer(VertexBuffer.TexCoordVertexBuffer.VertexBufferRHI);
#endif
		}
#else
		if (newNumIndices > 0)
		{
			auto& srcIndexBuffer = IndexBuffer.Indices;
			void* dstIndexBuffer = RHILockIndexBuffer(IndexBuffer.IndexBufferRHI, 0, newNumIndices * IndexTypeSize, RLM_WriteOnly);
			FMemory::Memcpy(dstIndexBuffer, srcIndexBuffer.GetData(), newNumIndices * IndexTypeSize);
			RHIUnlockIndexBuffer(IndexBuffer.IndexBufferRHI);
		}
		if (newNumVerts > 0)
		{
			auto& VertexBuffer = VertexBuffers.PositionVertexBuffer;
			void* VertexBufferData = RHILockVertexBuffer(VertexBuffer.VertexBufferRHI, 0, newNumVerts * PositionTypeSize, RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), newNumVerts * PositionTypeSize);
			RHIUnlockVertexBuffer(VertexBuffer.VertexBufferRHI);
		}

		// Leave the color buffer and tangents untouched as we don't need them
		/*
		{
			auto& VertexBuffer = VertexBuffers.ColorVertexBuffer;
			void* VertexBufferData = RHILockVertexBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
			RHIUnlockVertexBuffer(VertexBuffer.VertexBufferRHI);
		}

		{
			auto& VertexBuffer = VertexBuffers.StaticMeshVertexBuffer;
			void* VertexBufferData = RHILockVertexBuffer(VertexBuffer.TangentsVertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetTangentSize(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetTangentData(), VertexBuffer.GetTangentSize());
			RHIUnlockVertexBuffer(VertexBuffer.TangentsVertexBuffer.VertexBufferRHI);
		}
		*/
		if (newNumVerts > 0)
		{
			auto& VertexBuffer = VertexBuffers.StaticMeshVertexBuffer;
			void* VertexBufferData = RHILockVertexBuffer(VertexBuffer.TexCoordVertexBuffer.VertexBufferRHI, 0, newNumVerts * TexcoordTypeSize, RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetTexCoordData(), newNumVerts * TexcoordTypeSize);
			RHIUnlockVertexBuffer(VertexBuffer.TexCoordVertexBuffer.VertexBufferRHI);
		}
#endif


#if RHI_RAYTRACING
		if (IsRayTracingEnabled())
		{
			RayTracingGeometry.ReleaseResource();

			FRayTracingGeometryInitializer Initializer;
			Initializer.IndexBuffer = IndexBuffer.IndexBufferRHI;
			Initializer.TotalPrimitiveCount = newNumIndices / 3;
			Initializer.GeometryType = RTGT_Triangles;
			Initializer.bFastBuild = true;
			Initializer.bAllowUpdate = false;

			RayTracingGeometry.SetInitializer(Initializer);
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
			RayTracingGeometry.InitResource(RHICmdList);
#else
			RayTracingGeometry.InitResource();
#endif

			FRayTracingGeometrySegment Segment;
			Segment.VertexBuffer = VertexBuffers.PositionVertexBuffer.VertexBufferRHI;
			Segment.NumPrimitives = RayTracingGeometry.Initializer.TotalPrimitiveCount;
#if ENGINE_MAJOR_VERSION == 5
			Segment.MaxVertices = newNumVerts;
#endif
			RayTracingGeometry.Initializer.Segments.Add(Segment);
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
			RayTracingGeometry.UpdateRHI(RHICmdList);
#else
			RayTracingGeometry.UpdateRHI();
#endif
		}
#endif

		if (bNormalRender)
		{
			if (newNumVerts > 0)
			{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
				NormalRender_PositionVertexBuffer->UpdateRHI(RHICmdList);
#else
				NormalRender_PositionVertexBuffer->UpdateRHI();
#endif
			}

			if (newNumIndices > 0)
			{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
				NormalRender_IndexBuffer->UpdateRHI(RHICmdList);
#else
				NormalRender_IndexBuffer->UpdateRHI();
#endif
			}
		}
	}

	static void FilterWorldNormal_RenderThread(const FFlipFilterRenderTarget* FilterRenderTarget, const FTexture& SrcTexture, const FTexture& DstTexture, int FilterIteration, FRHICommandListImmediate& RHICmdList)
	{
		IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>("Renderer");
		FilterRenderTarget->DoFilter(SrcTexture, DstTexture, FilterIteration, RHICmdList, RendererModule);
        // For debugging only
        //RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
	}

	static void RenderWorldNormal_RenderThread(const FTexture& DestTexture, const FGenericDepthTarget* NormalRender_DepthTarget, 
		const FPositionOnlyVertexDeclaration* NormalRender_VertexDeclaration, const FPositionOnlyVertexBuffer* NormalRender_PositionVertexBuffer, const FDynamicMeshIndexBuffer32* NormalRender_IndexBuffer,
		FMatrix ObjectToCamera, FMatrix CameraToWorld, FMatrix ObjectToProjection, uint32 NumVerts, uint32 NumTriangles, FRHICommandListImmediate& RHICmdList)
	{
        // We don't linerize the vertex buffer RHI creation, so there's a chance this function is executed between the creation of
        // vertex buffer in main thread and the upload of vertex buffer in RHI thread.
        if (!NormalRender_PositionVertexBuffer->VertexBufferRHI)
            return;
        
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
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI();
			GraphicsPSOInit.DepthStencilTargetFormat = DestinationDepthTextureRHI->GetFormat();
			GraphicsPSOInit.DepthStencilAccess = FExclusiveDepthStencil::DepthWrite_StencilNop;
			// For Vulkan this has to be removed
//			GraphicsPSOInit.bDepthBounds = true;

			// New engine version...
			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FWorldNormalGenVS> VertexShader(ShaderMap);
			//VertexShader.GetVertexShader()->ShaderName = TEXT("FWorldNormalGenVS");
			TShaderMapRef<FWorldNormalGenPS> PixelShader(ShaderMap);
			//PixelShader.GetPixelShader()->ShaderName = TEXT("FWorldNormalGenPS");

			check(VertexShader.IsValid());
			check(PixelShader.IsValid());
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = NormalRender_VertexDeclaration->VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

#if ENGINE_MAJOR_VERSION >= 5
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
#else
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
#endif


			VertexShader->SetTransforms(RHICmdList, ObjectToCamera, ObjectToProjection);
			PixelShader->SetTransforms(RHICmdList, CameraToWorld);

			// Get RHI vertex buffer from mesh
			RHICmdList.SetStreamSource(0, NormalRender_PositionVertexBuffer->VertexBufferRHI, 0);

			// Get RHI index buffer from mesh
			RHICmdList.DrawIndexedPrimitive(
				NormalRender_IndexBuffer->IndexBufferRHI,
				/*BaseVertexIndex=*/ 0,
				/*MinIndex=*/ 0,
				/*NumVertices=*/NumVerts,
				/*StartIndex=*/ 0,
				/*NumPrimitives=*/NumTriangles,
				/*NumInstances=*/ 1
			);
		}
		RHICmdList.EndRenderPass();

		// Need this for Vulkan
		RHICmdList.Transition(FRHITransitionInfo(DestinationTextureRHI, ERHIAccess::RTV, ERHIAccess::SRVMask));

		// For debugging
		//RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);

	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		check(IsInRenderingThread());

		if (NumVerts <= 0 || NumIndices <= 0) 
		{
			return;
		}

		// Set up wireframe material (if needed)
		const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
		
		
		FColoredMaterialRenderProxy* WireframeMaterialInstance = NULL;
		if (bWireframe)
		{
			WireframeMaterialInstance = new FColoredMaterialRenderProxy(
				GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : NULL,
				FLinearColor(0, 0.5f, 1.f)
			);

			Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
		}

		FMaterialRenderProxy* UsedMaterialProxy = nullptr;
		// Grab material
		if (Material)
		{ 
			UsedMaterialProxy = Material->GetRenderProxy();
		}
		else
		{
			UMaterialInterface* MaterialInstance = UMaterial::GetDefaultMaterial(MD_Surface);
			// Get render proxy directly from MD_Surface material to avoid validity check
			UsedMaterialProxy = MaterialInstance->GetRenderProxy();
		}


		FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : UsedMaterialProxy;

		FMatrix ObjectToWorld = BaseComponent->GetComponentTransform().ToMatrixWithScale();

		

		
		// For each view..
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* pView = Views[ViewIndex];

				bool isInstancedStereo = pView->IsInstancedStereoPass();
				bool isFullPass = pView->StereoPass == EStereoscopicPass::eSSP_FULL;
				//const FSceneView& StereoEyeView = ViewFamily.GetStereoEyeView(pView->StereoPass);

				// TODO: is there a better way finding out whether it's in shadow rendering or not?
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

								RenderWorldNormal_RenderThread(*CaptureRenderTarget[renderTargetIndex]->GetResource(), NormalRender_DepthTarget.Get(),
									NormalRender_VertexDeclaration.Get(), NormalRender_PositionVertexBuffer.Get(), NormalRender_IndexBuffer.Get(),
									ObjectToCamera, CameraToWorld, ObjectToProjection, NumVerts, NumTriangles, RHICmdList);

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

				// Draw the mesh.
				FMeshBatch& Mesh = Collector.AllocateMesh();
				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				BatchElement.IndexBuffer = &IndexBuffer;
				Mesh.bWireframe = bWireframe;
				Mesh.VertexFactory = &VertexFactory;
				Mesh.MaterialRenderProxy = MaterialProxy;

				bool bHasPrecomputedVolumetricLightmap;
				FMatrix PreviousLocalToWorld;
				int32 SingleCaptureIndex;
				bool bOutputVelocity;
				GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);

				FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
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
				BatchElement.NumPrimitives = NumIndices / 3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = NumVerts - 1;
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = SDPG_World;
				Mesh.bCanApplyViewModeOverrides = false;
				Collector.AddMesh(ViewIndex, Mesh);
			}
		}

		// Draw bounds
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				// Render bounds
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
			}
		}
#endif
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
		return Result;
	}

	virtual bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual uint32 GetMemoryFootprint(void) const
	{
		return(sizeof(*this) + GetAllocatedSize());
	}

	uint32 GetAllocatedSize(void) const
	{
		return(FPrimitiveSceneProxy::GetAllocatedSize());
	}

#if RHI_RAYTRACING
	virtual bool IsRayTracingRelevant() const override { return true; }

#if ENGINE_MAJOR_VERSION == 5
	virtual bool HasRayTracingRepresentation() const override { return true; }
#endif

	virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances) override final
	{
		if (!Material)
			return;

		if (RayTracingGeometry.RayTracingGeometryRHI.IsValid())
		{
			check(RayTracingGeometry.Initializer.IndexBuffer.IsValid());

			FRayTracingInstance RayTracingInstance;
			RayTracingInstance.Geometry = &RayTracingGeometry;
			RayTracingInstance.InstanceTransforms.Add(GetLocalToWorld());

			uint32 SectionIdx = 0;
			FMeshBatch MeshBatch;

			MeshBatch.VertexFactory = &VertexFactory;
			MeshBatch.SegmentIndex = 0;
			MeshBatch.MaterialRenderProxy = Material->GetRenderProxy();
			MeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
			MeshBatch.Type = PT_TriangleList;
			MeshBatch.DepthPriorityGroup = SDPG_World;
			MeshBatch.bCanApplyViewModeOverrides = false;
			MeshBatch.CastRayTracedShadow = IsShadowCast(Context.ReferenceView);

			FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
			BatchElement.IndexBuffer = &IndexBuffer;

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
			BatchElement.NumPrimitives = IndexBuffer.Indices.Num() / 3;
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;

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

private:
	int32 NumVerts;
	int32 NumIndices;
	int32 NumTriangles;
	/** Material applied to this section */
	UMaterialInstanceDynamic* Material;
	/** Vertex buffer for this section */
	FStaticMeshVertexBuffers VertexBuffers;
	/** Index buffer for this section */
	FDynamicMeshIndexBuffer32 IndexBuffer;
	/** Vertex factory for this section */
	FLocalVertexFactory VertexFactory;

#if RHI_RAYTRACING
	FRayTracingGeometry RayTracingGeometry;
#endif

	FMaterialRelevance MaterialRelevance;

	/** For world space normal map rendering */
	const bool bNormalRender;
	TSharedPtr<FPositionOnlyVertexDeclaration> NormalRender_VertexDeclaration;
	TSharedPtr<FPositionOnlyVertexBuffer> NormalRender_PositionVertexBuffer;
	TSharedPtr<FDynamicMeshIndexBuffer32> NormalRender_IndexBuffer;
	TSharedPtr<FGenericDepthTarget> NormalRender_DepthTarget;
	TSharedPtr<FFlipFilterRenderTarget> NormalRender_FilterTarget;
	int32 NormalRender_SmoothIteration;

	TWeakObjectPtr<UTextureRenderTarget2D> CaptureRenderTarget[2];
	TWeakObjectPtr<UTextureRenderTarget2D> FilteredNormalRenderTarget[2];
	
	USceneComponent* BaseComponent;
};


UCortoMeshRendererComp::UCortoMeshRendererComp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true; // need this to tick and subsequently call MarkRenderTransformDirty()
	bUseAttachParentBound = false;
	m_meshUploader = std::make_shared<CortoDataUploader>(this);
	m_materialInstance = nullptr;

	m_mainTexture[0] = nullptr;
	m_mainTexture[1] = nullptr;
	m_mainTextureFirstFrame = nullptr;

	m_materialDirty = true;
}

UCortoMeshRendererComp::~UCortoMeshRendererComp()
{
}

std::shared_ptr<IEvercoastStreamingDataUploader> UCortoMeshRendererComp::GetMeshDataUploader() const
{
	return m_meshUploader;
}

void UCortoMeshRendererComp::SetMeshData(std::shared_ptr<CortoLocalMeshFrame> localMeshFrame)
{
	FCortoMeshSceneProxy* sceneProxy = (FCortoMeshSceneProxy*)(this->SceneProxy);
	if (!sceneProxy)
		return;

	m_currLocalMeshFrame = localMeshFrame;

	// Update bounds before sending data to scene proxy
	UpdateBounds();
	// Force scene to update primitive proxies bounds
	GetWorld()->Scene->UpdatePrimitiveTransform(this);

	std::shared_ptr<CortoLocalMeshFrame> meshFrame = localMeshFrame;
	sceneProxy->SetMeshData(meshFrame);
}

void UCortoMeshRendererComp::SetCortoMeshMaterial(UMaterialInterface* pMaterial)
{
	if (CortoMeshMaterial != pMaterial)
	{
		CortoMeshMaterial = pMaterial;
		m_materialInstance = CreateAndSetMaterialInstanceDynamicFromMaterial(0, pMaterial);
		CheckImageEnhanceMaterialParams();
		ApplyImageEnhanceMaterialParams();

		CheckEyeAdaptationCorrectionMaterialParams();
		ApplyEyeAdaptationCorrectionMaterialParams();

		MarkRenderStateDirty();

		m_materialDirty = true;
	}
}

void UCortoMeshRendererComp::SetGenerateWorldNormalSize(int32 size)
{
	GenerateWorldNormalSize = size;
	
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


void UCortoMeshRendererComp::SetFilteredWorldNormalSize(int32 size)
{
	FilteredWorldNormalSize = size;

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

static FName s_ImageEnhanceMaterialParameterNames[] = {
	FName(TEXT("Exposure_Compensation")),
	FName(TEXT("Hue_Shift_Percentage")),
	FName(TEXT("Contrast")),
	FName(TEXT("Brightness")),
	FName(TEXT("S_Curve_Power")),
	FName(TEXT("Additional_Gamma")),
};


static FName s_EyeAdaptationCorrectionMaterialParameterNames[] = {
	FName(TEXT("TexelEmission")),
	FName(TEXT("CorrectionIntensity")),
};

void UCortoMeshRendererComp::CommitImageEnhanceMaterialParams()
{
	if (!m_materialInstance)
	{
		m_materialInstance = CreateAndSetMaterialInstanceDynamicFromMaterial(0, CortoMeshMaterial);
	}

	CheckImageEnhanceMaterialParams();
	ApplyImageEnhanceMaterialParams();
}

void UCortoMeshRendererComp::CommitEyeAdaptationCorrectionMaterialParams()
{
	if (!m_materialInstance)
	{
		m_materialInstance = CreateAndSetMaterialInstanceDynamicFromMaterial(0, CortoMeshMaterial);
	}

	CheckEyeAdaptationCorrectionMaterialParams();
	ApplyEyeAdaptationCorrectionMaterialParams();
}

void UCortoMeshRendererComp::CheckImageEnhanceMaterialParams()
{
	if (m_materialInstance)
	{
		int expectedParamCount = sizeof(s_ImageEnhanceMaterialParameterNames) / sizeof(FName);
		int matchedParamCount = 0;
		// Check one parameter
		TArray<FMaterialParameterInfo> paramInfos;
		TArray<FGuid> paramGuid;
		m_materialInstance->GetAllScalarParameterInfo(paramInfos, paramGuid);
		for (auto& paramInfo : paramInfos)
		{
			for (int i = 0; i < expectedParamCount; ++i)
			{
				if (paramInfo.Name == s_ImageEnhanceMaterialParameterNames[i])
				{
					matchedParamCount++;
					break;
				}
			}
		}

		if (matchedParamCount >= expectedParamCount)
		{
			bIsImageEnhancementMaterial = true;
			return;
		}
	}

	bIsImageEnhancementMaterial = false;
}


void UCortoMeshRendererComp::CheckEyeAdaptationCorrectionMaterialParams()
{
	if (m_materialInstance)
	{
		int expectedParamCount = sizeof(s_EyeAdaptationCorrectionMaterialParameterNames) / sizeof(FName);
		int matchedParamCount = 0;
		// Check one parameter
		TArray<FMaterialParameterInfo> paramInfos;
		TArray<FGuid> paramGuid;
		m_materialInstance->GetAllScalarParameterInfo(paramInfos, paramGuid);
		for (auto& paramInfo : paramInfos)
		{
			for (int i = 0; i < expectedParamCount; ++i)
			{
				if (paramInfo.Name == s_EyeAdaptationCorrectionMaterialParameterNames[i])
				{
					matchedParamCount++;
					break;
				}
			}
		}

		if (matchedParamCount >= expectedParamCount)
		{
			bIsEyeAdaptationCorrectedMaterial = true;
			return;
		}
	}

	bIsEyeAdaptationCorrectedMaterial = false;
}


void UCortoMeshRendererComp::SetTextureData(std::shared_ptr<CortoLocalTextureFrame> localTexture)
{
	m_currLocalTextureFrame = localTexture;

	if (!m_materialInstance)
	{
		UMaterialInterface* pMaterial = CortoMeshMaterial;
		m_materialInstance = CreateAndSetMaterialInstanceDynamicFromMaterial(0, pMaterial);
		CheckImageEnhanceMaterialParams();
		ApplyImageEnhanceMaterialParams();

		CheckEyeAdaptationCorrectionMaterialParams();
		ApplyEyeAdaptationCorrectionMaterialParams();

		if (SceneProxy)
		{
			((FCortoMeshSceneProxy*)SceneProxy)->ResetMaterial(m_materialInstance);
		}
	}

	if (!m_materialInstance)
	{
		// allow this for editor to assign materials
		return;
	}

	auto texture = localTexture->GetTexture();
	check(texture && texture->GetResource());
	m_copyPromise = MakeShared<TPromise<void>, ESPMode::ThreadSafe>();
	auto future = m_copyPromise->GetFuture();

	// getting ready to make a copy
	int32 width = texture->GetSurfaceWidth();
	int32 height = texture->GetSurfaceHeight();
	EPixelFormat pixelFormat = PF_B8G8R8A8;
	if (localTexture->NeedsSwizzle())
	{
		pixelFormat = PF_R8G8B8A8;
	}

	UTexture2D*& mainTexture = m_mainTexture[m_mainTexturePtr]; // ref to ptr, otherwise we have to remember to assign the ptr back to the array or massive memory leak!
	if (!mainTexture || mainTexture->GetSurfaceWidth() != width || mainTexture->GetSurfaceHeight() != height || mainTexture->GetPixelFormat(0) != pixelFormat )
	{
		// It looks like you need to set a different name for transient texture so that when it's value 
		// was set into the material parameter, it will get properly updated! Crazy
		//FString name = FString::Printf(TEXT("CortoMeshRendererComp_MainTex%d"), m_mainTexturePtr);
		UObject* transientPackage = (UObject*)GetTransientPackage();

		FName mainTextureName = MakeUniqueObjectName(transientPackage, UTexture2D::StaticClass(), FName(TEXT("CortoMediaMainTex")));
		mainTexture = UTexture2D::CreateTransient(width, height, pixelFormat, mainTextureName);
		mainTexture->UpdateResource();
	}

	bool needUpdateFirstFrame = false;
	if (!m_mainTextureFirstFrame || m_mainTextureFirstFrame->GetSurfaceWidth() != width || m_mainTextureFirstFrame->GetSurfaceHeight() != height || m_mainTextureFirstFrame->GetPixelFormat(0) != pixelFormat)
	{
		UObject* transientPackage = (UObject*)GetTransientPackage();

		FName mainFirstTextureName = MakeUniqueObjectName(transientPackage, UTexture2D::StaticClass(), FName(TEXT("CortoMediaMainFirstTex")));
		m_mainTextureFirstFrame = UTexture2D::CreateTransient(width, height, pixelFormat, mainFirstTextureName);
		m_mainTextureFirstFrame->UpdateResource();

		needUpdateFirstFrame = true;
	}

	ENQUEUE_RENDER_COMMAND(UCortoMeshRendererComp_CopyMainTex)(
		[srcTex = texture, mainTex = mainTexture, self=this, promise = m_copyPromise, mainTextureFirstFrame = needUpdateFirstFrame ? m_mainTextureFirstFrame : nullptr](FRHICommandListImmediate& RHICmdList)
		{
			auto targetRHIRes = mainTex->GetResource();
			auto srcRHIRes = srcTex->GetResource();

			check(srcRHIRes);

			FTextureRHIRef targetRHI = targetRHIRes->TextureRHI;
			FTextureRHIRef srcRHI = srcRHIRes->TextureRHI;
			RHICmdList.Transition(FRHITransitionInfo(srcRHI, ERHIAccess::SRVMask, ERHIAccess::CopySrc));
			RHICmdList.Transition(FRHITransitionInfo(targetRHI, ERHIAccess::SRVMask, ERHIAccess::CopyDest));
			RHICmdList.CopyTexture(srcTex->GetResource()->TextureRHI, mainTex->GetResource()->TextureRHI, FRHICopyTextureInfo());
			RHICmdList.Transition(FRHITransitionInfo(targetRHI, ERHIAccess::CopyDest, ERHIAccess::SRVMask));

			if (mainTextureFirstFrame)
			{
				targetRHI = mainTextureFirstFrame->GetResource()->TextureRHI;
				RHICmdList.Transition(FRHITransitionInfo(targetRHI, ERHIAccess::SRVMask, ERHIAccess::CopyDest));
				RHICmdList.CopyTexture(srcTex->GetResource()->TextureRHI, mainTextureFirstFrame->GetResource()->TextureRHI, FRHICopyTextureInfo());
				RHICmdList.Transition(FRHITransitionInfo(targetRHI, ERHIAccess::CopyDest, ERHIAccess::SRVMask));
			}

			RHICmdList.Transition(FRHITransitionInfo(srcRHI, ERHIAccess::CopySrc, ERHIAccess::SRVMask));

			promise->SetValue();
		});

	future.Get();

	TArray< FMaterialParameterInfo > paramInfos;
	TArray< FGuid > paramIds;
//	m_materialInstance->ClearParameterValues();
	m_materialInstance->GetAllTextureParameterInfo(paramInfos, paramIds);
	if (paramInfos.Num() == 0)
	{
		UE_LOG(EvercoastRendererLog, Warning, TEXT("No texture parameter found in material. Cannot render."));
		return;
	}
	m_materialInstance->SetTextureParameterValue(TEXT("MainTex"), needUpdateFirstFrame ? m_mainTextureFirstFrame : mainTexture);
	m_materialInstance->SetScalarParameterValue(TEXT("WorldNormalFactor"), bGenerateNormal ? 1.0f : 0.0f);


	m_mainTexturePtr = (m_mainTexturePtr + 1) % 2;

}

UTexture* UCortoMeshRendererComp::GetTextureData()
{
	if (m_currLocalTextureFrame)
		return m_currLocalTextureFrame->GetTexture();
	return nullptr;
}

FPrimitiveSceneProxy* UCortoMeshRendererComp::CreateSceneProxy()
{
	if (!SceneProxy)
	{
		// whenever creates a new scene proxy, we mark both the voxel data and transform/bounds data dirty so that they gets updated in Tick()
		m_meshUploader->MarkDataDirty();

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

		if (bGenerateNormal && (!OverrideWorldNormalRenderTarget[0] || !OverrideWorldNormalRenderTarget[1]))
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

		UTextureRenderTarget2D *normalRenderTarget_Left, *normalRenderTarget_Right;
		if (OverrideWorldNormalRenderTarget[0] == nullptr || OverrideWorldNormalRenderTarget[1] == nullptr)
		{
			normalRenderTarget_Left = FilteredWorldNormalRenderTarget[0];
			normalRenderTarget_Right = FilteredWorldNormalRenderTarget[1];
		}
		else
		{
			normalRenderTarget_Left = OverrideWorldNormalRenderTarget[0];
			normalRenderTarget_Right = OverrideWorldNormalRenderTarget[1];
		}

		bool canGenerateNormal = bGenerateNormal && CaptureWorldNormalRenderTarget[0] != nullptr && CaptureWorldNormalRenderTarget[1] != nullptr && 
			CaptureWorldNormalRenderTarget[0]->GetSurfaceWidth() == CaptureWorldNormalRenderTarget[1]->GetSurfaceWidth() &&
			CaptureWorldNormalRenderTarget[0]->GetSurfaceHeight() == CaptureWorldNormalRenderTarget[1]->GetSurfaceHeight() &&
			normalRenderTarget_Left != nullptr && normalRenderTarget_Right != nullptr && 
			normalRenderTarget_Left->GetSurfaceWidth() == normalRenderTarget_Right->GetSurfaceWidth() &&
			normalRenderTarget_Left->GetSurfaceHeight() == normalRenderTarget_Right->GetSurfaceHeight() &&
			normalRenderTarget_Left->GetFormat() == normalRenderTarget_Right->GetFormat();

		auto newSceneProxy = new FCortoMeshSceneProxy(this, m_materialInstance, canGenerateNormal, WorldNormalSmoothIteration, CaptureWorldNormalRenderTarget[0], CaptureWorldNormalRenderTarget[1], normalRenderTarget_Left, normalRenderTarget_Right);
		if (m_currLocalMeshFrame)
		{
			newSceneProxy->SetMeshData(m_currLocalMeshFrame);
		}
		return newSceneProxy;
	}

	return SceneProxy;
}

int32 UCortoMeshRendererComp::GetNumMaterials() const
{
	if (m_materialInstance)
		return 1;

	return 0;
}

void UCortoMeshRendererComp::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (m_materialInstance)
	{
		OutMaterials.Add(m_materialInstance);

		if (m_materialDirty && SceneProxy)
		{
			((FCortoMeshSceneProxy*)SceneProxy)->ResetMaterial(m_materialInstance);
			m_materialDirty = false;
		}
	}
}

void UCortoMeshRendererComp::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (m_meshUploader->IsDataDirty())
	{
		m_meshUploader->ForceUpload();
	}
}


void UCortoMeshRendererComp::ApplyImageEnhanceMaterialParams()
{
	if (m_materialInstance && bIsImageEnhancementMaterial)
	{
		m_materialInstance->SetScalarParameterValue(FName(TEXT("Exposure_Compensation")), ExposureCompensation);
		m_materialInstance->SetScalarParameterValue(FName(TEXT("Hue_Shift_Percentage")), HueShiftPercentage);
		m_materialInstance->SetScalarParameterValue(FName(TEXT("Contrast")), Contrast);
		m_materialInstance->SetScalarParameterValue(FName(TEXT("Brightness")), Brightness);
		m_materialInstance->SetScalarParameterValue(FName(TEXT("S_Curve_Power")), SCurvePower);
		m_materialInstance->SetScalarParameterValue(FName(TEXT("Additional_Gamma")), AdditionalGamma);
	}
}


void UCortoMeshRendererComp::ApplyEyeAdaptationCorrectionMaterialParams()
{
	if (m_materialInstance && bIsEyeAdaptationCorrectedMaterial)
	{
		m_materialInstance->SetScalarParameterValue(FName(TEXT("TexelEmission")), PostEyeAdaptationCorrectionTexelEmission);
		m_materialInstance->SetScalarParameterValue(FName(TEXT("CorrectionIntensity")), EyeAdaptationCorrectionIntensity);
	}
}

#if WITH_EDITOR
void UCortoMeshRendererComp::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	CheckImageEnhanceMaterialParams();
	ApplyImageEnhanceMaterialParams();

	CheckEyeAdaptationCorrectionMaterialParams();
	ApplyEyeAdaptationCorrectionMaterialParams();

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

void UCortoMeshRendererComp::OnRegister()
{
	Super::OnRegister();
	MarkRenderStateDirty();
}



FBoxSphereBounds UCortoMeshRendererComp::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds bounds = CalcLocalBounds();

	return bounds.TransformBy(LocalToWorld);
}

FBoxSphereBounds UCortoMeshRendererComp::CalcLocalBounds() const
{
	if (m_currLocalMeshFrame)
	{
		return FBoxSphereBounds(m_currLocalMeshFrame->GetBounds());
	}

	return FBoxSphereBounds(FBox(FVector(-200, -50, -200), FVector(200, 300, 200)));;
}

void UCortoMeshRendererComp::OnUnregister()
{
	Super::OnUnregister();
}

void UCortoMeshRendererComp::BeginDestroy()
{
	m_meshUploader->ReleaseLocalResource();

	Super::BeginDestroy();
}

