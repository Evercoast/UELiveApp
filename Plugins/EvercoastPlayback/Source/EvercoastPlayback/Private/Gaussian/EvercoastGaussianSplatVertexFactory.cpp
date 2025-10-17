#include "Gaussian/EvercoastGaussianSplatVertexFactory.h"
#include "EvercoastVoxelDecoder.h" // log define
#include "ShaderCompilerCore.h"
#include "MeshDrawShaderBindings.h"
#include "RenderUtils.h"
#include "MeshMaterialShader.h"
#include "Gaussian/EvercoastGaussianSplatCSResult.h"
#include "Gaussian/GaussianSplatPreprocessComputeShader.h"
#include "Gaussian/InclusiveSumComputeShader.h"
#include "Gaussian/GaussianSplatAuxiliaryComputeShader.h"
#include "Gaussian/GaussianSplatTileRendererComputeShader.h"
#include "Gaussian/GaussianSplatComputeShaderConstants.h"
#include "Gaussian/GaussianSplatQuadRendererComputeShader.h"
#include "Gaussian/GaussianSplatTileRenderer.h"

#include "MaterialShared.h"
#include "GPUSort.h"
#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 2
#include "MaterialDomain.h"
#endif
#endif

// For easier version management in this file only
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#define __CreateUAV RHICmdList.CreateUnorderedAccessView
#define __CreateSRV RHICmdList.CreateShaderResourceView
#else
#define __CreateUAV RHICreateUnorderedAccessView
#define __CreateSRV RHICreateShaderResourceView
#endif


class FEvercoastGaussianSplatVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FEvercoastGaussianSplatVertexFactoryShaderParameters, NonVirtual);

public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		NumSplats.Bind(ParameterMap, TEXT("_NumSplats"), SPF_Mandatory);
		SortResultBufferIndex.Bind(ParameterMap, TEXT("_InstanceIdToSortId_Select"), SPF_Mandatory);
		SortValueListSRV_A.Bind(ParameterMap, TEXT("_InstanceIdToSortedId_A"), SPF_Mandatory);
		SortValueListSRV_B.Bind(ParameterMap, TEXT("_InstanceIdToSortedId_B"), SPF_Mandatory);
		SplatViewSRV.Bind(ParameterMap, TEXT("_SplatViewData"), SPF_Mandatory);
		ShadowBlobScale.Bind(ParameterMap, TEXT("_ShadowBlobScale"), SPF_Optional);
	}

	void GetElementShaderBindings(
		const class FSceneInterface* Scene,
		const FSceneView* View,
		const class FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams
	) const
	{
		const FEvercoastGaussianSplatVertexFactory* GaussianSplatVertexFactory = ((const FEvercoastGaussianSplatVertexFactory*)VertexFactory);

		FRHIUniformBuffer* VertexFactoryUniformBuffer = static_cast<FRHIUniformBuffer*>(BatchElement.VertexFactoryUserData);

		if (!VertexFactoryUniformBuffer)
		{
			// No batch element override
			VertexFactoryUniformBuffer = GaussianSplatVertexFactory->GetUniformBuffer();
		}
		// Bind default local vertex factory uniforms
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FLocalVertexFactoryUniformShaderParameters>(), VertexFactoryUniformBuffer);

		// bind vertex factory's SRV of RHIBuffer to relavant shader parameter
		int reconNumSplats = GaussianSplatVertexFactory->GetCurrentReconstructedNumSplats();
		ShaderBindings.Add(NumSplats, reconNumSplats);

		ShaderBindings.Add(SortValueListSRV_A, GaussianSplatVertexFactory->m_sortValueListSRV[0]);
		ShaderBindings.Add(SortValueListSRV_B, GaussianSplatVertexFactory->m_sortValueListSRV[1]);
		ShaderBindings.Add(SortResultBufferIndex, GaussianSplatVertexFactory->m_currSortResultBufferIndex);
		ShaderBindings.Add(SplatViewSRV, GaussianSplatVertexFactory->m_splatViewSRV);
		ShaderBindings.Add(ShadowBlobScale, GaussianSplatVertexFactory->m_shadowBlobScale);
	}
private:
	LAYOUT_FIELD(FShaderParameter, NumSplats);
	LAYOUT_FIELD(FShaderParameter, SortResultBufferIndex);
	LAYOUT_FIELD(FShaderResourceParameter, SortValueListSRV_A);
	LAYOUT_FIELD(FShaderResourceParameter, SortValueListSRV_B);
	LAYOUT_FIELD(FShaderResourceParameter, SplatViewSRV);
	LAYOUT_FIELD(FShaderParameter, ShadowBlobScale);
};


IMPLEMENT_TYPE_LAYOUT(FEvercoastGaussianSplatVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FEvercoastGaussianSplatVertexFactory, SF_Vertex, FEvercoastGaussianSplatVertexFactoryShaderParameters);

#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 5
IMPLEMENT_VERTEX_FACTORY_TYPE(FEvercoastGaussianSplatVertexFactory, "/EvercoastShaders/GaussianSplatLocalVertexFactory_5_5.ush",
	EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPrecisePrevWorldPos
	| EVertexFactoryFlags::SupportsPositionOnly
);

#elif ENGINE_MINOR_VERSION >= 4
IMPLEMENT_VERTEX_FACTORY_TYPE(FEvercoastGaussianSplatVertexFactory, "/EvercoastShaders/GaussianSplatLocalVertexFactory_5_4.ush",
	EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPrecisePrevWorldPos
	| EVertexFactoryFlags::SupportsPositionOnly
);

#elif ENGINE_MINOR_VERSION >= 2
IMPLEMENT_VERTEX_FACTORY_TYPE(FEvercoastGaussianSplatVertexFactory, "/EvercoastShaders/GaussianSplatLocalVertexFactory_5_2.ush",
	EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPrecisePrevWorldPos
	| EVertexFactoryFlags::SupportsPositionOnly
);
#else
#error Gaussian Splat needs Unreal Engine 5.2 and above!
#endif
#else
#error Gaussian Splat needs Unreal Engine 5.2 and above!
#endif


FEvercoastGaussianSplatVertexFactory::FEvercoastGaussianSplatVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const char* InDebugName) :
	FLocalVertexFactory(InFeatureLevel, InDebugName),
	m_numSplats(0),
	m_maxSplats(0),
	m_currSortResultBufferIndex(0),
	m_shadowBlobScale(1.0f),
	m_currReconstructedNumSplats(0)
	
{
}


bool FEvercoastGaussianSplatVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	if (Parameters.MaterialParameters.MaterialDomain == MD_Surface ||
		Parameters.MaterialParameters.bIsDefaultMaterial)
		return true;
	return false;
}

void FEvercoastGaussianSplatVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("EVERCOAST_GAUSSIANSPLAT"), TEXT("1"));
	// https://docs.unrealengine.com/4.27/en-US/ProgrammingAndScripting/Rendering/ShaderDevelopment/
	OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
}

void FEvercoastGaussianSplatVertexFactory::ReserveGaussianSplatRHI(std::shared_ptr<const EvercoastGaussianSplatCSResult> encodedGaussian)
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHILock);

	if (encodedGaussian)
	{
		// reserve splat RHI data and create them if necessary
		ReserveGaussianSplatCount(encodedGaussian->pointCount);
	}

	
}

void FEvercoastGaussianSplatVertexFactory::ReserveGaussianSplatCount(uint32_t inNumSplats)
{
	m_numSplats = inNumSplats;

	if (m_maxSplats < m_numSplats)
	{
		m_maxSplats = m_numSplats * 2;
		ReleaseGaussianSplatRHIResources();
		CreateGaussianSplatRHIResources();
	}
}

void FEvercoastGaussianSplatVertexFactory::CreateGaussianSplatRHIResources()
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHILock);

	FRHICommandListBase& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	for (uint32_t i = 0; i < GPU_SORT_BUFFER_COUNT; ++i)
	{
		FRHIResourceCreateInfo sortKeyListCreateInfo(*FString::Printf(TEXT("SortKeyListBuffer%d"), i));
		FRHIResourceCreateInfo sortValueListCreateInfo(*FString::Printf(TEXT("SortValueListBuffer%d"), i));

		m_sortKeyListBuffer[i] = RHICmdList.CreateBuffer(
			sizeof(uint32_t) * m_maxSplats,
			BUF_ShaderResource | BUF_UnorderedAccess | BUF_ByteAddressBuffer,
			sizeof(uint32_t),
			ERHIAccess::UAVMask, 
			sortKeyListCreateInfo
		);

		m_sortValueListBuffer[i] = RHICmdList.CreateBuffer(
			sizeof(uint32_t) * m_maxSplats,
			BUF_ShaderResource | BUF_UnorderedAccess | BUF_ByteAddressBuffer,
			sizeof(uint32_t),
			ERHIAccess::UAVMask, 
			sortValueListCreateInfo
		);
	}

	// Position buffer
	FRHIResourceCreateInfo EncodedSplatPositionCreateInfo(TEXT("EncodedSplatPositionBuffer"));
	m_encodedSplatPositionBuffer = RHICmdList.CreateBuffer(
		sizeof(EncodedSplatVector3) * m_maxSplats,
		BUF_ShaderResource | BUF_ByteAddressBuffer,
		sizeof(EncodedSplatVector3),
		ERHIAccess::SRVMask, 
		EncodedSplatPositionCreateInfo
	);

	// Colour + alpha buffer
	FRHIResourceCreateInfo EncodedSplatColourAlphaCreateInfo(TEXT("EncodedSplatColourAlphaBuffer"));
	m_encodedSplatColourAlphaBuffer = RHICmdList.CreateBuffer(
		sizeof(EncodedSplatColourAlpha) * m_maxSplats,
		BUF_ShaderResource | BUF_ByteAddressBuffer,
		sizeof(EncodedSplatColourAlpha),
		ERHIAccess::SRVMask,
		EncodedSplatColourAlphaCreateInfo
	);

	// Scale buffer
	FRHIResourceCreateInfo EncodedSplatScaleCreationInfo(TEXT("EncodedSplatScaleBuffer"));
	m_encodedSplatScaleBuffer = RHICmdList.CreateBuffer(
		sizeof(EncodedSplatScale) * m_maxSplats,
		BUF_ShaderResource | BUF_ByteAddressBuffer,
		sizeof(EncodedSplatScale),
		ERHIAccess::SRVMask,
		EncodedSplatScaleCreationInfo
	);

	// Rotation buffer
	FRHIResourceCreateInfo EncodedSplatRotationCreationInfo(TEXT("EncodedSplatRotationBuffer"));
	m_encodedSplatRotationBuffer = RHICmdList.CreateBuffer(
		sizeof(EncodedSplatRotation) * m_maxSplats,
		BUF_ShaderResource | BUF_ByteAddressBuffer,
		sizeof(EncodedSplatRotation),
		ERHIAccess::SRVMask,
		EncodedSplatRotationCreationInfo
	);

	// SH Coeffs buffer
	FRHIResourceCreateInfo EncodedSplatSHCoeffsCreationInfo(TEXT("EncodedSplatSHCoeffsBuffer"));
	m_encodedSplatSHCoeffsBuffer = RHICmdList.CreateBuffer(
		sizeof(EncodedSplat3DegreeSHCoeffs) * m_maxSplats,
		BUF_ShaderResource | BUF_ByteAddressBuffer,
		sizeof(EncodedSplat3DegreeSHCoeffs),
		ERHIAccess::SRVMask,
		EncodedSplatSHCoeffsCreationInfo
	);

	FRHIResourceCreateInfo SplatViewCreationInfo(TEXT("SplatViewBuffer"));

	// SplatView buffer
	m_splatViewBuffer = RHICmdList.CreateBuffer(
		sizeof(SplatView) * m_maxSplats,
		BUF_ShaderResource | BUF_UnorderedAccess | BUF_StructuredBuffer,
		sizeof(SplatView),
		ERHIAccess::UAVMask,
		SplatViewCreationInfo
	);


	// Views:
	for (uint32_t i = 0; i < GPU_SORT_BUFFER_COUNT; ++i)
	{
		m_sortKeyListUAV[i] = __CreateUAV(m_sortKeyListBuffer[i], false, false);
		m_sortKeyListSRV[i] = __CreateSRV(m_sortKeyListBuffer[i]);
		m_sortValueListUAV[i] = __CreateUAV(m_sortValueListBuffer[i], false, false);
		m_sortValueListSRV[i] = __CreateSRV(m_sortValueListBuffer[i]);
	}

	m_encodedSplatPositionSRV = __CreateSRV(m_encodedSplatPositionBuffer);
	m_encodedSplatColourAlphaSRV = __CreateSRV(m_encodedSplatColourAlphaBuffer);
	m_encodedSplatScaleSRV = __CreateSRV(m_encodedSplatScaleBuffer);
	m_encodedSplatRotationSRV = __CreateSRV(m_encodedSplatRotationBuffer);
	m_encodedSplatSHCoeffsSRV = __CreateSRV(m_encodedSplatSHCoeffsBuffer);

	m_splatViewUAV = __CreateUAV(m_splatViewBuffer, false, false);;
	m_splatViewSRV = __CreateSRV(m_splatViewBuffer);
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
void FEvercoastGaussianSplatVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
{
	FLocalVertexFactory::InitRHI(RHICmdList);
#else
void FEvercoastGaussianSplatVertexFactory::InitRHI()
{
	FLocalVertexFactory::InitRHI();
#endif
	// create dummy resource for just 1 splat

	check(IsInRenderingThread());
	ReserveGaussianSplatCount(1);
	

	m_currReconFrameIndex = 0;
	m_currReconstructedNumSplats = 0;
}


void FEvercoastGaussianSplatVertexFactory::ReleaseRHI()
{
	ReleaseGaussianSplatRHIResources();

	
	FLocalVertexFactory::ReleaseRHI();
}


void FEvercoastGaussianSplatVertexFactory::ReleaseGaussianSplatRHIResources()
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHILock);

	for (uint32_t i = 0; i < GPU_SORT_BUFFER_COUNT; ++i)
	{
		m_sortKeyListUAV[i].SafeRelease();
		m_sortKeyListSRV[i].SafeRelease();
		m_sortValueListUAV[i].SafeRelease();
		m_sortValueListSRV[i].SafeRelease();
	}

	m_encodedSplatPositionSRV.SafeRelease();
	m_encodedSplatColourAlphaSRV.SafeRelease();
	m_encodedSplatScaleSRV.SafeRelease();
	m_encodedSplatRotationSRV.SafeRelease();
	m_encodedSplatSHCoeffsSRV.SafeRelease();


	m_splatViewSRV.SafeRelease();
	m_splatViewUAV.SafeRelease();

	for (uint32_t i = 0; i < GPU_SORT_BUFFER_COUNT; ++i)
	{
		m_sortKeyListBuffer[i].SafeRelease();
		m_sortValueListBuffer[i].SafeRelease();
	}

	m_splatViewBuffer.SafeRelease();
	m_encodedSplatPositionBuffer.SafeRelease();
	m_encodedSplatColourAlphaBuffer.SafeRelease();
	m_encodedSplatScaleBuffer.SafeRelease();
	m_encodedSplatRotationBuffer.SafeRelease();
	m_encodedSplatSHCoeffsBuffer.SafeRelease();
}

/**
 * Execute a GPU sort test.
 * @param TestSize - The number of elements to sort.
 * @returns true if the sort succeeded.
 */
static bool RunGPUSortTest(FRHICommandListImmediate& RHICmdList, int32 TestSize, ERHIFeatureLevel::Type FeatureLevel)
{
	FRandomStream RandomStream(0x3819FFE4);
	FGPUSortBuffers SortBuffers;
	TArray<uint32> Keys;
	TArray<uint32> Values;
	TArray<uint32> RefSortedKeys;
	TArray<uint32> RefSortedValues;
	TArray<uint32> SortedKeys;
	TArray<uint32> SortedValues;
	FBufferRHIRef KeysBufferRHI[2], ValuesBufferRHI[2];
	FShaderResourceViewRHIRef KeysBufferSRV[2], ValuesBufferSRV[2];
	FUnorderedAccessViewRHIRef KeysBufferUAV[2], ValuesBufferUAV[2];
	int32 ResultBufferIndex;
	int32 IncorrectKeyIndex = 0;
	const int32 BufferSize = TestSize * sizeof(uint32);

	// Generate the test keys.
	Keys.Reserve(TestSize);
	Keys.AddUninitialized(TestSize);
	Values.Reserve(TestSize);
	Values.AddUninitialized(TestSize);
	for (int32 KeyIndex = 0; KeyIndex < TestSize; ++KeyIndex)
	{
		Keys[KeyIndex] = RandomStream.GetUnsignedInt();
		Values[KeyIndex] = KeyIndex;//RandomStream.GetUnsignedInt();
	}

	// Perform a reference sort on the CPU.
	RefSortedKeys = Keys;
	RefSortedKeys.Sort();
	RefSortedValues = Values;
	RefSortedValues.Sort();

	// Allocate GPU resources.
	for (int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex)
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("KeysBuffer"));
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		KeysBufferRHI[BufferIndex] = RHICmdList.CreateVertexBuffer(BufferSize, BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess, CreateInfo);
#else
		KeysBufferRHI[BufferIndex] = RHICreateVertexBuffer(BufferSize, BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess, CreateInfo);
#endif
		KeysBufferSRV[BufferIndex] = RHICmdList.CreateShaderResourceView(KeysBufferRHI[BufferIndex], /*Stride=*/ sizeof(uint32), PF_R32_UINT);
		KeysBufferUAV[BufferIndex] = RHICmdList.CreateUnorderedAccessView(KeysBufferRHI[BufferIndex], PF_R32_UINT);
		CreateInfo.DebugName = TEXT("ValuesBuffer");
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
		ValuesBufferRHI[BufferIndex] = RHICmdList.CreateVertexBuffer(BufferSize, BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess, CreateInfo);
#else
		ValuesBufferRHI[BufferIndex] = RHICreateVertexBuffer(BufferSize, BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess, CreateInfo);
#endif
		ValuesBufferSRV[BufferIndex] = RHICmdList.CreateShaderResourceView(ValuesBufferRHI[BufferIndex], /*Stride=*/ sizeof(uint32), PF_R32_UINT);
		ValuesBufferUAV[BufferIndex] = RHICmdList.CreateUnorderedAccessView(ValuesBufferRHI[BufferIndex], PF_R32_UINT);
	}

	// Upload initial keys and values to the GPU.
	{
		uint32* Buffer;

		Buffer = (uint32*)RHICmdList.LockBuffer(KeysBufferRHI[0], /*Offset=*/ 0, BufferSize, RLM_WriteOnly);
		FMemory::Memcpy(Buffer, Keys.GetData(), BufferSize);
		RHICmdList.UnlockBuffer(KeysBufferRHI[0]);
		Buffer = (uint32*)RHICmdList.LockBuffer(ValuesBufferRHI[0], /*Offset=*/ 0, BufferSize, RLM_WriteOnly);
		FMemory::Memcpy(Buffer, Values.GetData(), BufferSize);
		RHICmdList.UnlockBuffer(ValuesBufferRHI[0]);
	}

	// Execute the GPU sort.
	for (int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex)
	{
		SortBuffers.RemoteKeySRVs[BufferIndex] = KeysBufferSRV[BufferIndex];
		SortBuffers.RemoteKeyUAVs[BufferIndex] = KeysBufferUAV[BufferIndex];
		SortBuffers.RemoteValueSRVs[BufferIndex] = ValuesBufferSRV[BufferIndex];
		SortBuffers.RemoteValueUAVs[BufferIndex] = ValuesBufferUAV[BufferIndex];
	}
	ResultBufferIndex = SortGPUBuffers(RHICmdList, SortBuffers, 0, 0xFFFFFFFF, TestSize, FeatureLevel);

	// Download results from the GPU.
	{
		uint32* Buffer;

		SortedKeys.Reserve(TestSize);
		SortedKeys.AddUninitialized(TestSize);
		SortedValues.Reserve(TestSize);
		SortedValues.AddUninitialized(TestSize);

		Buffer = (uint32*)RHICmdList.LockBuffer(KeysBufferRHI[ResultBufferIndex], /*Offset=*/ 0, BufferSize, RLM_ReadOnly);
		FMemory::Memcpy(SortedKeys.GetData(), Buffer, BufferSize);
		RHICmdList.UnlockBuffer(KeysBufferRHI[ResultBufferIndex]);
		Buffer = (uint32*)RHICmdList.LockBuffer(ValuesBufferRHI[ResultBufferIndex], /*Offset=*/ 0, BufferSize, RLM_ReadOnly);
		FMemory::Memcpy(SortedValues.GetData(), Buffer, BufferSize);
		RHICmdList.UnlockBuffer(ValuesBufferRHI[ResultBufferIndex]);
	}

	// Verify results.
	bool bSucceeded = true;
	for (int32 KeyIndex = 0; KeyIndex < TestSize; ++KeyIndex)
	{
		if (SortedKeys[KeyIndex] != RefSortedKeys[KeyIndex] || SortedValues[KeyIndex] != RefSortedValues[KeyIndex])
		{
			IncorrectKeyIndex = KeyIndex;
			bSucceeded = false;
			break;
		}
	}

	if (bSucceeded)
	{
		UE_LOG(EvercoastVoxelDecoderLog, Log, TEXT("GPU Sort Test (%d keys+values) succeeded."), TestSize);
	}
	else
	{
		UE_LOG(EvercoastVoxelDecoderLog, Log, TEXT("GPU Sort Test (%d keys+values) FAILED."), TestSize);

	}

	return bSucceeded;
}

void FEvercoastGaussianSplatVertexFactory::PerformComputeShaderSplatDataReconForQuadRenderer(const FMatrix& ObjectToWorld, const FMatrix& InView, const FMatrix& InProj, 
	const FVector& InCameraPositionWS, const FVector4& InScreenParam, bool isShadowPass, float splatDecimation, float splatExtraScale, float cov2DSqrtKernelSize, 
	bool showSH0Colour, bool showSH1Colour, bool showSH2Colour, bool showSH3Colour, std::shared_ptr<const EvercoastGaussianSplatCSResult> encodedGaussian)
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHILock);

	if (!encodedGaussian)
		return;

	// If it's just in-between a ReleaseRHI and a InitRHI, then all the RHI resources may still missing a InitRHI() call
	// Normally this function will only be called within RHI thread. However it will be called on main thread when 
	// EvercoastGaussianSplatCSRendererComp::bReconstructOnTickOnly is set to true
	if (!m_encodedSplatPositionBuffer)
		return;

	ENQUEUE_RENDER_COMMAND(FDispatchGaussianSplatCompute)( [
			&accessRHILock = this->m_accessRHILock,
			&accessMetadataLock = this->m_accessMetadataLock,

			sortKeyListUAV_A = m_sortKeyListUAV[0],
			sortKeyListUAV_B = m_sortKeyListUAV[1],
			sortKeyListSRV_A = m_sortKeyListSRV[0],
			sortKeyListSRV_B = m_sortKeyListSRV[1],

			sortValueListUAV_A = m_sortValueListUAV[0],
			sortValueListUAV_B = m_sortValueListUAV[1],
			sortValueListSRV_A = m_sortValueListSRV[0],
			sortValueListSRV_B = m_sortValueListSRV[1],


			retainedEncodedSplatData = encodedGaussian,
			encodedSplatPositionBuffer = m_encodedSplatPositionBuffer,
			encodedSplatPositionSRV = m_encodedSplatPositionSRV,
			encodedSplatColourAlphaBuffer = m_encodedSplatColourAlphaBuffer,
			encodedSplatColourAlphaSRV = m_encodedSplatColourAlphaSRV,
			encodedSplatScaleBuffer = m_encodedSplatScaleBuffer,
			encodedSplatScaleSRV = m_encodedSplatScaleSRV,
			encodedSplatRotationBuffer = m_encodedSplatRotationBuffer,
			encodedSplatRotationSRV = m_encodedSplatRotationSRV,
			encodedSplatSHCoeffsBuffer = m_encodedSplatSHCoeffsBuffer,
			encodedSplatSHCoeffsSRV = m_encodedSplatSHCoeffsSRV,
			splatViewUAV = m_splatViewUAV,
			ObjectToWorld = ObjectToWorld,
			View = InView,
			Proj = InProj,
			CameraPositionWS = InCameraPositionWS,
			ScreenParam = InScreenParam,
			IsShadowPass = isShadowPass,
			Decimation = splatDecimation,
			SplatExtraScale = splatExtraScale,
			Cov2DSqrtKernelSize = cov2DSqrtKernelSize,
			showSH0 = showSH0Colour,
			showSH1 = showSH1Colour,
			showSH2 = showSH2Colour,
			showSH3 = showSH3Colour,
	
			&resultBufferIndex = this->m_currSortResultBufferIndex,
			&currReconstructedNumSplats = this->m_currReconstructedNumSplats,
			&currReconstructedFrameIndex = this->m_currReconFrameIndex] (FRHICommandListImmediate& RHICmdList)
		{
			if (!retainedEncodedSplatData)
				return;

			uint32_t splatCount = retainedEncodedSplatData->pointCount;

			std::lock_guard<std::recursive_mutex> guard(accessRHILock);


			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			uint32 ThreadGroupCount;

			// First initialize sorting data
			RHICmdList.Transition(FRHITransitionInfo(sortValueListUAV_A, ERHIAccess::Unknown, ERHIAccess::UAVMask));

			TShaderMapRef<FGaussianSplatInitSortDataCS> InitSortDataCS(ShaderMap);

			SetComputePipelineState(RHICmdList, InitSortDataCS.GetComputeShader());

			// Bind
			FGaussianSplatInitSortDataCS::FParameters ShaderParameters;
			ShaderParameters.SplatCount = splatCount;
			ShaderParameters.SortValueList_A = sortValueListUAV_A;
			SetShaderParameters(RHICmdList, InitSortDataCS, InitSortDataCS.GetComputeShader(), ShaderParameters);

			// Dispatch
			ThreadGroupCount = FMath::DivideAndRoundUp<uint32>(splatCount, INIT_SORTING_THREADS);
			RHICmdList.DispatchComputeShader(ThreadGroupCount, 1, 1);

			// Unbind
			UnsetShaderUAVs(RHICmdList, InitSortDataCS, InitSortDataCS.GetComputeShader());

			// Then decode & calculate splat view data
			// Upload position data
			void* PositionBufferData = RHICmdList.LockBuffer(encodedSplatPositionBuffer, 0, sizeof(EncodedSplatVector3) * splatCount, RLM_WriteOnly);
			FMemory::Memcpy(PositionBufferData, retainedEncodedSplatData->packedPositions, retainedEncodedSplatData->packedPositionsSize);
			RHICmdList.UnlockBuffer(encodedSplatPositionBuffer);

			// Upload colour & alpha data
			void* ColourAlphaBufferData = RHICmdList.LockBuffer(encodedSplatColourAlphaBuffer, 0, sizeof(EncodedSplatColourAlpha) * splatCount, RLM_WriteOnly);
			FMemory::Memcpy(ColourAlphaBufferData, retainedEncodedSplatData->packedColourAlphas, retainedEncodedSplatData->packedColourAlphasSize);
			RHICmdList.UnlockBuffer(encodedSplatColourAlphaBuffer);

			// Upload scale data
			void* ScaleBufferData = RHICmdList.LockBuffer(encodedSplatScaleBuffer, 0, sizeof(EncodedSplatScale) * splatCount, RLM_WriteOnly);
			FMemory::Memcpy(ScaleBufferData, retainedEncodedSplatData->packedScales, retainedEncodedSplatData->packedScalesSize);
			RHICmdList.UnlockBuffer(encodedSplatScaleBuffer);

			// Upload rotation data
			void* RotationBufferData = RHICmdList.LockBuffer(encodedSplatRotationBuffer, 0, sizeof(EncodedSplatRotation) * splatCount, RLM_WriteOnly);
			FMemory::Memcpy(RotationBufferData, retainedEncodedSplatData->packedRotations, retainedEncodedSplatData->packedRotationsSize);
			RHICmdList.UnlockBuffer(encodedSplatRotationBuffer);

			// Upload SH coeffs data
			void* SHCoeffsBufferData = RHICmdList.LockBuffer(encodedSplatSHCoeffsBuffer, 0, sizeof(EncodedSplat3DegreeSHCoeffs) * splatCount, RLM_WriteOnly);
			FMemory::Memcpy(SHCoeffsBufferData, retainedEncodedSplatData->packedSHCoeffs, retainedEncodedSplatData->packedSHCoeffsSize);
			RHICmdList.UnlockBuffer(encodedSplatSHCoeffsBuffer);
			

			RHICmdList.Transition(FRHITransitionInfo(encodedSplatPositionBuffer, ERHIAccess::Unknown, ERHIAccess::SRVMask));
			RHICmdList.Transition(FRHITransitionInfo(encodedSplatColourAlphaBuffer, ERHIAccess::Unknown, ERHIAccess::SRVMask));
			RHICmdList.Transition(FRHITransitionInfo(encodedSplatScaleBuffer, ERHIAccess::Unknown, ERHIAccess::SRVMask));
			RHICmdList.Transition(FRHITransitionInfo(encodedSplatRotationBuffer, ERHIAccess::Unknown, ERHIAccess::SRVMask));
			RHICmdList.Transition(FRHITransitionInfo(encodedSplatSHCoeffsBuffer, ERHIAccess::Unknown, ERHIAccess::SRVMask));
			RHICmdList.Transition(FRHITransitionInfo(splatViewUAV, ERHIAccess::Unknown, ERHIAccess::SRVMask));

			TShaderMapRef<FGaussianSplatQuadRendererPreprocessComputeShader> CalcSplatViewDataCS(ShaderMap);


			SetComputePipelineState(RHICmdList, CalcSplatViewDataCS.GetComputeShader());

			FMatrix WorldToObject = ObjectToWorld.Inverse();
			uint32_t shDim = (retainedEncodedSplatData->shDegree + 1) * (retainedEncodedSplatData->shDegree + 1) - 1;
			// Bind UAV
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
			FRHIBatchedShaderParameters& BatchedShaderParams2 = RHICmdList.GetScratchShaderParameters();

			CalcSplatViewDataCS->SetupTransformsAndUniforms(BatchedShaderParams2, ObjectToWorld, WorldToObject,
				splatCount,
				retainedEncodedSplatData->shDegree,
				shDim,
				retainedEncodedSplatData->positionScalar,
				View, Proj, CameraPositionWS, ScreenParam, IsShadowPass, Decimation, SplatExtraScale, Cov2DSqrtKernelSize,
				showSH0, showSH1, showSH2, showSH3
			);
			CalcSplatViewDataCS->SetupIOBuffers(BatchedShaderParams2, encodedSplatPositionSRV, encodedSplatColourAlphaSRV, encodedSplatScaleSRV, encodedSplatRotationSRV, encodedSplatSHCoeffsSRV, splatViewUAV, sortKeyListUAV_A);
			RHICmdList.SetBatchedShaderParameters(CalcSplatViewDataCS.GetComputeShader(), BatchedShaderParams2);
#else
			
			// Transform data only can get from SceneProxy::GetDynamicMeshElements, so this function is called with up-to-date view data
			CalcSplatViewDataCS->SetupTransformsAndUniforms(RHICmdList, ObjectToWorld, WorldToObject,
				splatCount,
				retainedEncodedSplatData->shDegree,
				shDim,
				retainedEncodedSplatData->positionScalar,
				View, Proj, CameraPositionWS, ScreenParam, IsShadowPass, Decimation, SplatExtraScale, Cov2DSqrtKernelSize,
				showSH0, showSH1, showSH2, showSH3
			);
			CalcSplatViewDataCS->SetupIOBuffers(RHICmdList, encodedSplatPositionSRV, encodedSplatColourAlphaSRV, encodedSplatScaleSRV, encodedSplatRotationSRV, encodedSplatSHCoeffsSRV, splatViewUAV, sortKeyListUAV_A);
#endif

			// Dispatch
			ThreadGroupCount = FMath::DivideAndRoundUp<uint32>(splatCount, CALC_VIEW_DATA_THREADS);
			RHICmdList.DispatchComputeShader(ThreadGroupCount, 1, 1);

			// Unbind
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
			if (RHICmdList.NeedsShaderUnbinds())
#endif
			{
				FRHIBatchedShaderUnbinds& BatchedUnbinds = RHICmdList.GetScratchShaderUnbinds();
				CalcSplatViewDataCS->UnbindBuffers(BatchedUnbinds);
				RHICmdList.SetBatchedShaderUnbinds(CalcSplatViewDataCS.GetComputeShader(), BatchedUnbinds);
			}
#else
			CalcSplatViewDataCS->UnbindBuffers(RHICmdList);
#endif

			// Quad renderer to perform GPU sort here, not to confused with the GPU sort in the second stage of the tile renderer.
			// GPU Radix sort, element count is different
			if (!IsShadowPass)
			{


				// Transition resource for reading (optional depending on next usage)
				RHICmdList.Transition(FRHITransitionInfo(splatViewUAV, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
				RHICmdList.Transition(FRHITransitionInfo(sortKeyListUAV_A, ERHIAccess::Unknown, ERHIAccess::UAVMask));
				RHICmdList.Transition(FRHITransitionInfo(sortKeyListUAV_B, ERHIAccess::Unknown, ERHIAccess::UAVMask));
				RHICmdList.Transition(FRHITransitionInfo(sortValueListUAV_A, ERHIAccess::Unknown, ERHIAccess::UAVMask));
				RHICmdList.Transition(FRHITransitionInfo(sortValueListUAV_B, ERHIAccess::Unknown, ERHIAccess::UAVMask));

				FGPUSortBuffers SortBuffers; // fill in buffers
				SortBuffers.RemoteKeySRVs[0] = sortKeyListSRV_A;
				SortBuffers.RemoteKeySRVs[1] = sortKeyListSRV_B;
				SortBuffers.RemoteValueSRVs[0] = sortValueListSRV_A;
				SortBuffers.RemoteValueSRVs[1] = sortValueListSRV_B;

				SortBuffers.RemoteKeyUAVs[0] = sortKeyListUAV_A;
				SortBuffers.RemoteKeyUAVs[1] = sortKeyListUAV_B;
				SortBuffers.RemoteValueUAVs[0] = sortValueListUAV_A;
				SortBuffers.RemoteValueUAVs[1] = sortValueListUAV_B;


				// Run gpu sorter
				resultBufferIndex = SortGPUBuffers(RHICmdList, SortBuffers, 0, 0xFFFFFFFF, (int32)splatCount, GMaxRHIFeatureLevel);

				//RHICmdList.Transition(FRHITransitionInfo(sortValueListUAV_A, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
				//RHICmdList.Transition(FRHITransitionInfo(sortValueListUAV_B, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
				// Sort key list for VF shader to read
				if (resultBufferIndex == 0)
				{
					RHICmdList.Transition(FRHITransitionInfo(sortValueListUAV_A, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
					RHICmdList.Transition(FRHITransitionInfo(sortKeyListUAV_A, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
				}
				else
				{
					RHICmdList.Transition(FRHITransitionInfo(sortValueListUAV_B, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
					RHICmdList.Transition(FRHITransitionInfo(sortKeyListUAV_B, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
				}
			}
			else
			{
				resultBufferIndex = 0;
				RHICmdList.Transition(FRHITransitionInfo(sortValueListUAV_A, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
			}

			{
				std::lock_guard<std::recursive_mutex> guard2(accessMetadataLock);

				currReconstructedNumSplats = splatCount;
				currReconstructedFrameIndex = retainedEncodedSplatData->frameIndex;
			}

			// fence?
	});

	// wait for fence?
}

void FEvercoastGaussianSplatVertexFactory::SetShadowBlobScale(float scale)
{
	m_shadowBlobScale = scale;
}