#pragma once

#include <mutex>
#include "CoreMinimal.h"
#include "EvercoastGaussianSplatVertexFactory.h"
#include "PrimitiveSceneProxy.h"
#include "DynamicMeshBuilder.h"
#include "Gaussian/EvercoastGaussianSplatCSRendererComp.h"

#if RHI_RAYTRACING
#include "RayTracingDefinitions.h"
#include "RayTracingInstance.h"
#endif

class UEvercoastGaussianSplatCSRendererComp;
class EvercoastGaussianSplatCSResult;
class UMaterialInstanceDynamic;
class UTextureRenderTarget2D;
class FGaussianSplatTileRenderer;

class FEvercoastGaussianSplatSceneProxy : public FPrimitiveSceneProxy
{
public:
	FEvercoastGaussianSplatSceneProxy(const UEvercoastGaussianSplatCSRendererComp* component, UMaterialInterface* material,
		EGaussianSplatRendererType rendererType,
		float splatDecimation, float splatExtraScale, float cov2DSqrtKernelSize, bool showDiffuseColour, bool showSH1Colour, bool showSH2Colour, bool showSH3Colour,
		bool enableTileRendererDepthWrite, EGaussianSplatHookStage tileRendererHookStage, float InAlphaCutoutThreshold);
	virtual ~FEvercoastGaussianSplatSceneProxy();

	/** Return a type (or subtype) specific hash for sorting purposes */
	virtual SIZE_T GetTypeHash() const;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

#if RHI_RAYTRACING
	virtual bool IsRayTracingRelevant() const override { return true; }

#if ENGINE_MAJOR_VERSION == 5
	virtual bool HasRayTracingRepresentation() const override { return true; }
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
	virtual void GetDynamicRayTracingInstances(FRayTracingInstanceCollector& Collector) override;
#else
	virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances) override;
#endif
#endif

	virtual uint32 GetMemoryFootprint(void) const override;
	uint32 GetAllocatedSize(void) const;
	void SetEncodedGaussianSplat_RenderThread(FRHICommandListBase& RHICmdList, std::shared_ptr<const EvercoastGaussianSplatCSResult> data);

	void ResetMaterial(UMaterialInterface* material);

	static FBoxSphereBounds GetDefaultLocalBounds();
	FBoxSphereBounds GetLocalBounds() const;

	void LockGaussianData();
	void UnlockGaussianData();

	void SaveEssentialReconData(const FMatrix& ObjectToWorld, const FMatrix& InView, const FMatrix& InProj, const FVector& InCameraPositionWS, 
		const FVector4& InScreenParam, bool isShadowPass, float InDecimation, float InSplatExtraScale, float InCov2DSqrtKernelSize, 
		bool showSH0Colour, bool showSH1Colour, bool showSH2Colour, bool showSH3Colour, const FVector4& InDepthOutputThreshold, std::shared_ptr<const EvercoastGaussianSplatCSResult> InGaussianData) const;
	void PerformLateComputeShaderSplatRecon();

	bool bPerformLateComputeShaderSplatRecon;

	void SetSplatDecimation(float decimation);
	void SetSplatExtraScale(float scale);
	void SetCov2DSqrtKernelSize(float kernelSize);
	void SetShadowBlobScale(float scale);

	void SetShowSphericalHarmonics0(bool show);
	void SetShowSphericalHarmonics1(bool show);
	void SetShowSphericalHarmonics2(bool show);
	void SetShowSphericalHarmonics3(bool show);

	void SetRendererType(EGaussianSplatRendererType newType);
	void EnableTileRendererDepthWrite(bool enableDepthWrite);
	void SetTileRendererHookStage(EGaussianSplatHookStage stage);
	void SetAlphaCutoutThreshold(float InAlphaCutout);

protected:
	// Return regular camera view matrix
	virtual const FViewMatrices& ExtractRelevantViewMatrices(const FSceneView* pView) const;
	virtual bool ShouldSubmitDynamicMesh(const FSceneView* pView) const;
private:

	void InitialiseQuadMesh();

	void PerformDataReconForTileRenderer(const FMatrix& InObjectToWorld, const FMatrix& InView, const FMatrix& InProj,
		const FVector& InCameraPositionWS, const FVector4& InScreenParam, float InCov2DSqrtKernelSize,
		bool showSH0Colour, bool showSH1Colour, bool showSH2Colour, bool showSH3Colour, const FVector4& InDepthOutputThreshold,
		std::shared_ptr<const EvercoastGaussianSplatCSResult> encodedGaussian) const;

	// remove constantness requirement in GetDynamicMeshElements() const
	mutable FEvercoastGaussianSplatVertexFactory m_vertexFactory;

#if PLATFORM_WINDOWS
	// Tile renderer is here, it no longer relies on vertex factory now
	TSharedPtr<FGaussianSplatTileRenderer> m_tileRenderer;
#endif

	// Splats data
	std::shared_ptr<const EvercoastGaussianSplatCSResult> m_encodedGaussian;
	mutable std::recursive_mutex	m_gaussianFrameLock;

	FStaticMeshVertexBuffers m_quadVertexBuffers;
	FDynamicMeshIndexBuffer32 m_quadIndexBuffer;

	UMaterialInterface* m_material;

	EGaussianSplatRendererType m_rendererType;
	EGaussianSplatHookStage m_tileRendererHookStage;

	float m_splatDecimation;
	float m_splatExtraScale;
	float m_cov2DSqrtKernelSize;
	bool m_splatShowDiffuse;
	bool m_splatShowSH1;
	bool m_splatShowSH2;
	bool m_splatShowSH3;
	bool m_tileRendererDepthWrite;
	FVector4 m_depthOutputThreshold; // x = enable depth write, w = alpha cutout threshold

	/** The view relevance for the gaussian material. Critical for GetViewRelevance() */
	FMaterialRelevance MaterialRelevance;

	// Cached data for recon
	mutable FMatrix SavedObjectToWorld;
	mutable FMatrix SavedView;
	mutable FMatrix SavedProj;
	mutable FVector SavedCameraPositionWS;
	mutable FVector4 SavedScreenParam;
	mutable bool SavedIsShadowPass;
	mutable float SavedDecimation;
	mutable float SavedSplatExtraScale;
	mutable float SavedCov2DSqrtKernelSize;
	mutable bool SavedShowSHColour[4];
	mutable FVector4 SavedDepthOutputThreshold;
	mutable std::shared_ptr<const EvercoastGaussianSplatCSResult> SavedEncodedGaussian;

#if RHI_RAYTRACING
	FRayTracingGeometry RayTracingGeometry;
#endif
};