#include "Gaussian/EvercoastGaussianSplatVertexFactory.h"
#include "EvercoastVoxelDecoder.h" // log define
#include "ShaderCompilerCore.h"
#include "MeshDrawShaderBindings.h"
#include "RenderUtils.h"
#include "MeshMaterialShader.h"
#include "Gaussian/EvercoastGaussianSplatPassthroughResult.h"
#include "Gaussian/GaussianSplatComputeShader.h"
#include "MaterialShared.h"
#include "GPUSort.h"
#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 2
#include "MaterialDomain.h"
#endif
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

		// TODO: bind vertex factory's SRV of RHIBuffer to relavant shader parameter
		ShaderBindings.Add(NumSplats, GaussianSplatVertexFactory->m_numSplats);
		ShaderBindings.Add(SortValueListSRV_A, GaussianSplatVertexFactory->m_sortValueListSRV[0]);
		ShaderBindings.Add(SortValueListSRV_B, GaussianSplatVertexFactory->m_sortValueListSRV[1]);
		ShaderBindings.Add(SortResultBufferIndex, GaussianSplatVertexFactory->m_currSortResultBufferIndex);
		ShaderBindings.Add(SplatViewSRV, GaussianSplatVertexFactory->m_splatViewSRV);
	}
private:
	LAYOUT_FIELD(FShaderParameter, NumSplats);
	LAYOUT_FIELD(FShaderParameter, SortResultBufferIndex);
	LAYOUT_FIELD(FShaderResourceParameter, SortValueListSRV_A);
	LAYOUT_FIELD(FShaderResourceParameter, SortValueListSRV_B);
	LAYOUT_FIELD(FShaderResourceParameter, SplatViewSRV);
};


IMPLEMENT_TYPE_LAYOUT(FEvercoastGaussianSplatVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FEvercoastGaussianSplatVertexFactory, SF_Vertex, FEvercoastGaussianSplatVertexFactoryShaderParameters);


#if ENGINE_MAJOR_VERSION >= 5

#if ENGINE_MINOR_VERSION >= 4
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
#endif
#else
#error Gaussian Splat needs Unreal Engine 5.2 and above!
#endif


FEvercoastGaussianSplatVertexFactory::FEvercoastGaussianSplatVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const char* InDebugName) :
	FLocalVertexFactory(InFeatureLevel, InDebugName),
	m_numSplats(0),
	m_maxSplats(0),
	m_currSortResultBufferIndex(0)
{

}


bool FEvercoastGaussianSplatVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
#if 0
	// HACKHACK: FOR DEBUG FAST ITERATION
	return false;
#else
	if (Parameters.MaterialParameters.MaterialDomain == MD_Surface ||
		Parameters.MaterialParameters.bIsDefaultMaterial)
		return true;
	return false;
#endif
}

void FEvercoastGaussianSplatVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("EVERCOAST_GAUSSIANSPLAT"), TEXT("1"));
	// https://docs.unrealengine.com/4.27/en-US/ProgrammingAndScripting/Rendering/ShaderDevelopment/
	OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
}

void FEvercoastGaussianSplatVertexFactory::SetEncodedGaussianSplatData(std::shared_ptr<const EvercoastGaussianSplatPassthroughResult> encodedGaussian)
{
	m_encodedGaussian = encodedGaussian;
	if (m_encodedGaussian)
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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	FRHICommandListBase& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	for (uint32_t i = 0; i < GPU_SORT_BUFFER_COUNT; ++i)
	{
		FRHIResourceCreateInfo sortKeyListCreateInfo(*FString::Printf(TEXT("SortKeyListBuffer%d"), i));
		FRHIResourceCreateInfo sortValueListCreateInfo(*FString::Printf(TEXT("SortValueListBuffer%d"), i));

		m_sortKeyListBuffer[i] = RHICmdList.CreateBuffer(
			sizeof(uint32_t) * m_maxSplats,
			BUF_ShaderResource | BUF_UnorderedAccess | BUF_ByteAddressBuffer,
			sizeof(uint32_t),
			ERHIAccess::SRVMask, //| ERHIAccess::UAVMask, // avoid UE5.3 writeable mask assert
			sortKeyListCreateInfo
		);

		m_sortValueListBuffer[i] = RHICmdList.CreateBuffer(
			sizeof(uint32_t) * m_maxSplats,
			BUF_ShaderResource | BUF_UnorderedAccess | BUF_ByteAddressBuffer,
			sizeof(uint32_t),
			ERHIAccess::SRVMask, //| ERHIAccess::UAVMask, // avoid UE5.3 writeable mask assert
			sortValueListCreateInfo
		);
	}

	// Position buffer
	FRHIResourceCreateInfo EncodedSplatPositionCreateInfo(TEXT("EncodedSplatPositionBuffer"));
	m_encodedSplatPositionBuffer = RHICmdList.CreateBuffer(
		sizeof(EncodedSplatVector3) * m_maxSplats,
		BUF_ShaderResource | BUF_UnorderedAccess | BUF_ByteAddressBuffer,
		sizeof(EncodedSplatVector3),
		ERHIAccess::SRVMask, // | ERHIAccess::UAVMask, // avoid UE5.3 writeable mask assert
		EncodedSplatPositionCreateInfo
	);

	// Colour + alpha buffer
	FRHIResourceCreateInfo EncodedSplatColourAlphaCreateInfo(TEXT("EncodedSplatColourAlphaBuffer"));
	m_encodedSplatColourAlphaBuffer = RHICmdList.CreateBuffer(
		sizeof(EncodedSplatColourAlpha) * m_maxSplats,
		BUF_ShaderResource | BUF_UnorderedAccess | BUF_ByteAddressBuffer,
		sizeof(EncodedSplatColourAlpha),
		ERHIAccess::SRVMask, // | ERHIAccess::UAVMask, // avoid UE5.3 writeable mask assert
		EncodedSplatColourAlphaCreateInfo
	);

	// Scale buffer
	FRHIResourceCreateInfo EncodedSplatScaleCreationInfo(TEXT("EncodedSplatScaleBuffer"));
	m_encodedSplatScaleBuffer = RHICmdList.CreateBuffer(
		sizeof(EncodedSplatScale) * m_maxSplats,
		BUF_ShaderResource | BUF_UnorderedAccess | BUF_ByteAddressBuffer,
		sizeof(EncodedSplatScale),
		ERHIAccess::SRVMask, // | ERHIAccess::UAVMask, // avoid UE5.3 writeable mask assert
		EncodedSplatScaleCreationInfo
	);

	// Rotation buffer
	FRHIResourceCreateInfo EncodedSplatRotationCreationInfo(TEXT("EncodedSplatRotationBuffer"));
	m_encodedSplatRotationBuffer = RHICmdList.CreateBuffer(
		sizeof(EncodedSplatRotation) * m_maxSplats,
		BUF_ShaderResource | BUF_UnorderedAccess | BUF_ByteAddressBuffer,
		sizeof(EncodedSplatRotation),
		ERHIAccess::SRVMask, // | ERHIAccess::UAVMask, // avoid UE5.3 writeable mask assert
		EncodedSplatRotationCreationInfo
	);

	FRHIResourceCreateInfo SplatViewCreationInfo(TEXT("SplatViewBuffer"));

	// SplatView buffer
	m_splatViewBuffer = RHICmdList.CreateBuffer(
		sizeof(SplatView) * m_maxSplats,
		BUF_ShaderResource | BUF_UnorderedAccess | BUF_StructuredBuffer,
		sizeof(SplatView),
		ERHIAccess::SRVMask, // | ERHIAccess::UAVCompute, // avoid UE5.3 writeable mask assert
		SplatViewCreationInfo
	);

	// Views:
	for (uint32_t i = 0; i < GPU_SORT_BUFFER_COUNT; ++i)
	{
		m_sortKeyListUAV[i] = RHICmdList.CreateUnorderedAccessView(m_sortKeyListBuffer[i], false, false);
		m_sortKeyListSRV[i] = RHICmdList.CreateShaderResourceView(m_sortKeyListBuffer[i]);
		m_sortValueListUAV[i] = RHICmdList.CreateUnorderedAccessView(m_sortValueListBuffer[i], false, false);
		m_sortValueListSRV[i] = RHICmdList.CreateShaderResourceView(m_sortValueListBuffer[i]);
	}

	m_encodedSplatPositionUAV = RHICmdList.CreateUnorderedAccessView(m_encodedSplatPositionBuffer, false, false);
	m_encodedSplatColourAlphaUAV = RHICmdList.CreateUnorderedAccessView(m_encodedSplatColourAlphaBuffer, false, false);
	m_encodedSplatScaleUAV = RHICmdList.CreateUnorderedAccessView(m_encodedSplatScaleBuffer, false, false);
	m_encodedSplatRotationUAV = RHICmdList.CreateUnorderedAccessView(m_encodedSplatRotationBuffer, false, false);

	m_splatViewUAV = RHICmdList.CreateUnorderedAccessView(m_splatViewBuffer, false, false);;
	m_splatViewSRV = RHICmdList.CreateShaderResourceView(m_splatViewBuffer);

#else

	// create all RHI objects with capacity of m_numSplats
	// Create StructuredBuffer on GPU
	// Buffers:
	// Sort key and value
	for (uint32_t i = 0; i < GPU_SORT_BUFFER_COUNT; ++i)
	{
		FRHIResourceCreateInfo sortKeyListCreateInfo(*FString::Printf(TEXT("SortKeyListBuffer%d"), i));
		FRHIResourceCreateInfo sortValueListCreateInfo(*FString::Printf(TEXT("SortValueListBuffer%d"), i));

		m_sortKeyListBuffer[i] = RHICreateBuffer(
			sizeof(uint32_t) * m_maxSplats,
			BUF_ShaderResource | BUF_UnorderedAccess | BUF_ByteAddressBuffer,
			sizeof(uint32_t),
			ERHIAccess::SRVMask | ERHIAccess::UAVMask,
			sortKeyListCreateInfo
		);

		m_sortValueListBuffer[i] = RHICreateBuffer(
			sizeof(uint32_t) * m_maxSplats,
			BUF_ShaderResource | BUF_UnorderedAccess | BUF_ByteAddressBuffer,
			sizeof(uint32_t),
			ERHIAccess::SRVMask | ERHIAccess::UAVMask,
			sortValueListCreateInfo
		);
	}


	// Position buffer
	FRHIResourceCreateInfo EncodedSplatPositionCreateInfo(TEXT("EncodedSplatPositionBuffer"));
	m_encodedSplatPositionBuffer = RHICreateBuffer(
		sizeof(EncodedSplatVector3) * m_maxSplats,
		BUF_ShaderResource | BUF_UnorderedAccess | BUF_ByteAddressBuffer,
		sizeof(EncodedSplatVector3),
		ERHIAccess::SRVMask | ERHIAccess::UAVMask,
		EncodedSplatPositionCreateInfo
	);

	// Colour + alpha buffer
	FRHIResourceCreateInfo EncodedSplatColourAlphaCreateInfo(TEXT("EncodedSplatColourAlphaBuffer"));
	m_encodedSplatColourAlphaBuffer = RHICreateBuffer(
		sizeof(EncodedSplatColourAlpha) * m_maxSplats,
		BUF_ShaderResource | BUF_UnorderedAccess | BUF_ByteAddressBuffer,
		sizeof(EncodedSplatColourAlpha),
		ERHIAccess::SRVMask | ERHIAccess::UAVMask,
		EncodedSplatColourAlphaCreateInfo
	);

	// Scale buffer
	FRHIResourceCreateInfo EncodedSplatScaleCreationInfo(TEXT("EncodedSplatScaleBuffer"));
	m_encodedSplatScaleBuffer = RHICreateBuffer(
		sizeof(EncodedSplatScale) * m_maxSplats,
		BUF_ShaderResource | BUF_UnorderedAccess | BUF_ByteAddressBuffer,
		sizeof(EncodedSplatScale),
		ERHIAccess::SRVMask | ERHIAccess::UAVMask,
		EncodedSplatScaleCreationInfo
	);

	// Rotation buffer
	FRHIResourceCreateInfo EncodedSplatRotationCreationInfo(TEXT("EncodedSplatRotationBuffer"));
	m_encodedSplatRotationBuffer = RHICreateBuffer(
		sizeof(EncodedSplatRotation) * m_maxSplats,
		BUF_ShaderResource | BUF_UnorderedAccess | BUF_ByteAddressBuffer,
		sizeof(EncodedSplatRotation),
		ERHIAccess::SRVMask | ERHIAccess::UAVMask,
		EncodedSplatRotationCreationInfo
	);

	FRHIResourceCreateInfo SplatViewCreationInfo(TEXT("SplatViewBuffer"));

	// SplatView buffer
	m_splatViewBuffer = RHICreateBuffer(
		sizeof(SplatView) * m_maxSplats,
		BUF_ShaderResource | BUF_UnorderedAccess | BUF_StructuredBuffer,
		sizeof(SplatView),
		ERHIAccess::SRVMask | ERHIAccess::UAVCompute,
		SplatViewCreationInfo
	);

	// Views:
	for (uint32_t i = 0; i < GPU_SORT_BUFFER_COUNT; ++i)
	{
		m_sortKeyListUAV[i] = RHICreateUnorderedAccessView(m_sortKeyListBuffer[i], false, false);
		m_sortKeyListSRV[i] = RHICreateShaderResourceView(m_sortKeyListBuffer[i]);
		m_sortValueListUAV[i] = RHICreateUnorderedAccessView(m_sortValueListBuffer[i], false, false);
		m_sortValueListSRV[i] = RHICreateShaderResourceView(m_sortValueListBuffer[i]);
	}

	m_encodedSplatPositionUAV = RHICreateUnorderedAccessView(m_encodedSplatPositionBuffer, false, false);
	m_encodedSplatColourAlphaUAV = RHICreateUnorderedAccessView(m_encodedSplatColourAlphaBuffer, false, false);
	m_encodedSplatScaleUAV = RHICreateUnorderedAccessView(m_encodedSplatScaleBuffer, false, false);
	m_encodedSplatRotationUAV = RHICreateUnorderedAccessView(m_encodedSplatRotationBuffer, false, false);

	m_splatViewUAV = RHICreateUnorderedAccessView(m_splatViewBuffer, false, false);;
	m_splatViewSRV = RHICreateShaderResourceView(m_splatViewBuffer);
#endif
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

	ReserveGaussianSplatCount(200000);
	
}


void FEvercoastGaussianSplatVertexFactory::ReleaseRHI()
{
	ReleaseGaussianSplatRHIResources();

	FLocalVertexFactory::ReleaseRHI();
}


void FEvercoastGaussianSplatVertexFactory::ReleaseGaussianSplatRHIResources()
{
	// TODO: make sure the render queue isn't using them
	for (uint32_t i = 0; i < GPU_SORT_BUFFER_COUNT; ++i)
	{
		m_sortKeyListUAV[i].SafeRelease();
		m_sortKeyListSRV[i].SafeRelease();
		m_sortValueListUAV[i].SafeRelease();
		m_sortValueListSRV[i].SafeRelease();
	}

	m_encodedSplatPositionUAV.SafeRelease();
	m_encodedSplatColourAlphaUAV.SafeRelease();
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



void FEvercoastGaussianSplatVertexFactory::PerformComputeShaderSplatDataRecon(const FMatrix& InObjectToWorld, const FVector& InPreViewTranslation, const FMatrix& InViewProj, const FMatrix& InView, const FMatrix& InProj, const FVector4& InScreenParam, const FMatrix& InClipToWorld, bool InIsShadowPass)
{
	ENQUEUE_RENDER_COMMAND(FDispatchGaussianSplatCompute)(
		[
			splatCount = m_numSplats,
			sortKeyListUAV_A = m_sortKeyListUAV[0],
			sortKeyListUAV_B = m_sortKeyListUAV[1],
			sortKeyListSRV_A = m_sortKeyListSRV[0],
			sortKeyListSRV_B = m_sortKeyListSRV[1],

			sortValueListUAV_A = m_sortValueListUAV[0],
			sortValueListUAV_B = m_sortValueListUAV[1],
			sortValueListSRV_A = m_sortValueListSRV[0],
			sortValueListSRV_B = m_sortValueListSRV[1],

			// Output buffer select index
			&resultBufferIndex = this->m_currSortResultBufferIndex,

			// DEBUG
//			sortKeyListBuffer_A = m_sortKeyListBuffer[0],
//			sortKeyListBuffer_B = m_sortKeyListBuffer[1],
//			sortValueListBuffer_A = m_sortValueListBuffer[0],
//			sortValueListBuffer_B = m_sortValueListBuffer[1],
			
			retainedEncodedSplatData = m_encodedGaussian,
			encodedSplatPositionBuffer = m_encodedSplatPositionBuffer,
			encodedSplatPositionUAV = m_encodedSplatPositionUAV,
			encodedSplatColourAlphaBuffer = m_encodedSplatColourAlphaBuffer,
			encodedSplatColourAlphaUAV = m_encodedSplatColourAlphaUAV,
			encodedSplatScaleBuffer = m_encodedSplatScaleBuffer,
			encodedSplatScaleUAV = m_encodedSplatScaleUAV,
			encodedSplatRotationBuffer = m_encodedSplatRotationBuffer,
			encodedSplatRotationUAV = m_encodedSplatRotationUAV,
			splatViewUAV = m_splatViewUAV,
			ObjectToWorld = InObjectToWorld,
			PreViewTranslation = InPreViewTranslation,
			ViewProj = InViewProj,
			View = InView,
			Proj = InProj,
			ScreenParam = InScreenParam,
			ClipToWorld = InClipToWorld,
			IsShadowPass = InIsShadowPass
		] (FRHICommandListImmediate& RHICmdList)
		{
			if (!retainedEncodedSplatData)
				return;

			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			uint32 ThreadGroupCount;

			// First initialize sorting data
			RHICmdList.Transition(FRHITransitionInfo(sortKeyListUAV_A, ERHIAccess::Unknown, ERHIAccess::UAVMask));
			RHICmdList.Transition(FRHITransitionInfo(sortKeyListUAV_B, ERHIAccess::Unknown, ERHIAccess::UAVMask));
			RHICmdList.Transition(FRHITransitionInfo(sortValueListUAV_A, ERHIAccess::Unknown, ERHIAccess::UAVMask));
			RHICmdList.Transition(FRHITransitionInfo(sortValueListUAV_B, ERHIAccess::Unknown, ERHIAccess::UAVMask));

			TShaderMapRef<FGaussianSplatInitSortDataCS> InitSortDataCS(ShaderMap);

			SetComputePipelineState(RHICmdList, InitSortDataCS.GetComputeShader());

			// Bind UAV
			InitSortDataCS->SetupUniforms(RHICmdList, retainedEncodedSplatData->pointCount);
			InitSortDataCS->SetupIOBuffers(RHICmdList, sortKeyListUAV_A, sortKeyListUAV_B, sortValueListUAV_A, sortValueListUAV_B);

			// Dispatch
			ThreadGroupCount = FMath::DivideAndRoundUp<uint32>(splatCount, 128);
			RHICmdList.DispatchComputeShader(ThreadGroupCount, 1, 1);

			// Unbind
			InitSortDataCS->UnbindBuffers(RHICmdList);
			

			// Then decode & calculate splat view data
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
			// Upload position data
			void* PositionBufferData = RHICmdList.LockBuffer(encodedSplatPositionBuffer, 0, sizeof(EncodedSplatVector3) * retainedEncodedSplatData->pointCount, RLM_WriteOnly);
			FMemory::Memcpy(PositionBufferData, retainedEncodedSplatData->packedPositions, retainedEncodedSplatData->packedPositionsSize);
			RHICmdList.UnlockBuffer(encodedSplatPositionBuffer);

			// Upload colour & alpha data
			void* ColourAlphaBufferData = RHICmdList.LockBuffer(encodedSplatColourAlphaBuffer, 0, sizeof(EncodedSplatColourAlpha) * retainedEncodedSplatData->pointCount, RLM_WriteOnly);
			FMemory::Memcpy(ColourAlphaBufferData, retainedEncodedSplatData->packedColourAlphas, retainedEncodedSplatData->packedColourAlphasSize);
			RHICmdList.UnlockBuffer(encodedSplatColourAlphaBuffer);

			// Upload scale data
			void* ScaleBufferData = RHICmdList.LockBuffer(encodedSplatScaleBuffer, 0, sizeof(EncodedSplatScale) * retainedEncodedSplatData->pointCount, RLM_WriteOnly);
			FMemory::Memcpy(ScaleBufferData, retainedEncodedSplatData->packedScales, retainedEncodedSplatData->packedScalesSize);
			RHICmdList.UnlockBuffer(encodedSplatScaleBuffer);

			// Upload rotation data
			void* RotationBufferData = RHICmdList.LockBuffer(encodedSplatRotationBuffer, 0, sizeof(EncodedSplatRotation) * retainedEncodedSplatData->pointCount, RLM_WriteOnly);
			FMemory::Memcpy(RotationBufferData, retainedEncodedSplatData->packedRotations, retainedEncodedSplatData->packedRotationsSize);
			RHICmdList.UnlockBuffer(encodedSplatRotationBuffer);
#else
			// Upload position data
			void* PositionBufferData = RHILockBuffer(encodedSplatPositionBuffer, 0, sizeof(EncodedSplatVector3) * retainedEncodedSplatData->pointCount, RLM_WriteOnly);
			FMemory::Memcpy(PositionBufferData, retainedEncodedSplatData->packedPositions, retainedEncodedSplatData->packedPositionsSize);
			RHIUnlockBuffer(encodedSplatPositionBuffer);

			// Upload colour & alpha data
			void* ColourAlphaBufferData = RHILockBuffer(encodedSplatColourAlphaBuffer, 0, sizeof(EncodedSplatColourAlpha) * retainedEncodedSplatData->pointCount, RLM_WriteOnly);
			FMemory::Memcpy(ColourAlphaBufferData, retainedEncodedSplatData->packedColourAlphas, retainedEncodedSplatData->packedColourAlphasSize);
			RHIUnlockBuffer(encodedSplatColourAlphaBuffer);

			// Upload scale data
			void* ScaleBufferData = RHILockBuffer(encodedSplatScaleBuffer, 0, sizeof(EncodedSplatScale) * retainedEncodedSplatData->pointCount, RLM_WriteOnly);
			FMemory::Memcpy(ScaleBufferData, retainedEncodedSplatData->packedScales, retainedEncodedSplatData->packedScalesSize);
			RHIUnlockBuffer(encodedSplatScaleBuffer);

			// Upload rotation data
			void* RotationBufferData = RHILockBuffer(encodedSplatRotationBuffer, 0, sizeof(EncodedSplatRotation) * retainedEncodedSplatData->pointCount, RLM_WriteOnly);
			FMemory::Memcpy(RotationBufferData, retainedEncodedSplatData->packedRotations, retainedEncodedSplatData->packedRotationsSize);
			RHIUnlockBuffer(encodedSplatRotationBuffer);
#endif
			// TODO: upload SH to differernt RWStructuredBuffers

			RHICmdList.Transition(FRHITransitionInfo(encodedSplatPositionUAV, ERHIAccess::Unknown, ERHIAccess::UAVMask));
			RHICmdList.Transition(FRHITransitionInfo(encodedSplatColourAlphaUAV, ERHIAccess::Unknown, ERHIAccess::UAVMask));
			RHICmdList.Transition(FRHITransitionInfo(encodedSplatScaleUAV, ERHIAccess::Unknown, ERHIAccess::UAVMask));
			RHICmdList.Transition(FRHITransitionInfo(encodedSplatRotationUAV, ERHIAccess::Unknown, ERHIAccess::UAVMask));
			RHICmdList.Transition(FRHITransitionInfo(splatViewUAV, ERHIAccess::Unknown, ERHIAccess::UAVMask));
//			RHICmdList.Transition(FRHITransitionInfo(sortKeyListUAV_A, ERHIAccess::Unknown, ERHIAccess::UAVMask));
//			RHICmdList.Transition(FRHITransitionInfo(sortKeyListUAV_B, ERHIAccess::Unknown, ERHIAccess::UAVMask));

			TShaderMapRef<FGaussianSplatComputeShader> ComputeShader(ShaderMap);


			SetComputePipelineState(RHICmdList, ComputeShader.GetComputeShader());

			// Bind UAV
			// Transform data only can get from SceneProxy::GetDynamicMeshElements, so this function is called with up-to-date view data
			ComputeShader->SetupTransformsAndUniforms(RHICmdList, ObjectToWorld, PreViewTranslation, ViewProj,
				retainedEncodedSplatData->pointCount, 
				retainedEncodedSplatData->shDegree,
				retainedEncodedSplatData->positionScalar,
				View, Proj, ScreenParam, ClipToWorld, IsShadowPass
				);
			ComputeShader->SetupIOBuffers(RHICmdList, encodedSplatPositionUAV, encodedSplatColourAlphaUAV, encodedSplatScaleUAV, encodedSplatRotationUAV, splatViewUAV, sortKeyListUAV_A, sortKeyListUAV_B);

			// Dispatch
			ThreadGroupCount = FMath::DivideAndRoundUp<uint32>(splatCount, 128);
			RHICmdList.DispatchComputeShader(ThreadGroupCount, 1, 1);

			// Unbind
			ComputeShader->UnbindBuffers(RHICmdList);

			// Transition resource for reading (optional depending on next usage)
			RHICmdList.Transition(FRHITransitionInfo(splatViewUAV, ERHIAccess::UAVMask, ERHIAccess::SRVMask));

//			RHICmdList.Transition(FRHITransitionInfo(sortValueListUAV_A, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
//			RHICmdList.Transition(FRHITransitionInfo(sortValueListUAV_B, ERHIAccess::UAVMask, ERHIAccess::SRVMask));

			// With Z_view data converted to uint and written to SortValueList buffer
			// now it's time to call sorter to sort both key and value list so that the VF shader can pick the right order up
			FGPUSortBuffers SortBuffers;
			for (int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex)
			{
				SortBuffers.RemoteKeySRVs[BufferIndex] = (BufferIndex == 0) ? sortKeyListSRV_A : sortKeyListSRV_B;
				SortBuffers.RemoteKeyUAVs[BufferIndex] = (BufferIndex == 0) ? sortKeyListUAV_A : sortKeyListUAV_B;
				SortBuffers.RemoteValueSRVs[BufferIndex] = (BufferIndex == 0) ? sortValueListSRV_A : sortValueListSRV_B;
				SortBuffers.RemoteValueUAVs[BufferIndex] = (BufferIndex == 0) ? sortValueListUAV_A : sortValueListUAV_B;

			}
			// Run gpu sorter
			resultBufferIndex = SortGPUBuffers(RHICmdList, SortBuffers, 0, 0xFFFFFFFF, (int32)retainedEncodedSplatData->pointCount, GMaxRHIFeatureLevel);

			
			//RHICmdList.Transition(FRHITransitionInfo(sortValueListUAV_A, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
			//RHICmdList.Transition(FRHITransitionInfo(sortValueListUAV_B, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
			// Sort key list for VF shader to read
			if (resultBufferIndex == 0)
			{
				RHICmdList.Transition(FRHITransitionInfo(sortValueListUAV_A, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
			}
			else
			{
				RHICmdList.Transition(FRHITransitionInfo(sortValueListUAV_B, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
			}
		});
}