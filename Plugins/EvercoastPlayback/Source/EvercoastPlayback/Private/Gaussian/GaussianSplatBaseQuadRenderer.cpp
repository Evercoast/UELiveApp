#include "Gaussian/GaussianSplatBaseQuadRenderer.h"
#include "Gaussian/EvercoastGaussianSplatCSResult.h"
#include "Gaussian/GaussianSplatPreprocessComputeShader.h"
#include "Gaussian/GaussianSplatQuadRendererComputeShader.h"
#include "GPUSort.h"
#include "CommonRenderResources.h"
#include "EngineModule.h"


// For easier version management in this file only
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#define __CreateUAV RHICmdList.CreateUnorderedAccessView
#define __CreateSRV RHICmdList.CreateShaderResourceView
#define __CreateVertexBuffer RHICmdList.CreateVertexBuffer
#else
#define __CreateUAV RHICreateUnorderedAccessView
#define __CreateSRV RHICreateShaderResourceView
#define __CreateVertexBuffer RHICreateVertexBuffer
#endif


static bool RunGPUSortTest(FRHICommandListImmediate& RHICmdList, int32 TestSize, ERHIFeatureLevel::Type FeatureLevel)
{
	FRandomStream RandomStream(0x3819FFE4);
	FGPUSortBuffers SortBuffers;
	TArray<uint32> Keys;
	TArray<uint32> RefSortedKeys;
	TArray<uint32> SortedKeys;
	TArray<uint32> SortedValues;
	FBufferRHIRef KeysBufferRHI[2], ValuesBufferRHI[2];
	FShaderResourceViewRHIRef KeysBufferSRV[2], ValuesBufferSRV[2];
	FUnorderedAccessViewRHIRef KeysBufferUAV[2], ValuesBufferUAV[2];
	int32 ResultBufferIndex;
	int32 IncorrectKeyIndex = 0;
	const int32 BufferSize = TestSize * sizeof(uint32);
	const bool bDebugSort = true;

	// Generate the test keys.
	Keys.Reserve(TestSize);
	Keys.AddUninitialized(TestSize);
	for (int32 KeyIndex = 0; KeyIndex < TestSize; ++KeyIndex)
	{
		Keys[KeyIndex] = RandomStream.GetUnsignedInt();
	}

	// Perform a reference sort on the CPU.
	RefSortedKeys = Keys;
	RefSortedKeys.Sort();

	// Allocate GPU resources.
	for (int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex)
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("KeysBuffer"));
		KeysBufferRHI[BufferIndex] = __CreateVertexBuffer(BufferSize, BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess, CreateInfo);
		KeysBufferSRV[BufferIndex] = RHICmdList.CreateShaderResourceView(KeysBufferRHI[BufferIndex], /*Stride=*/ sizeof(uint32), PF_R32_UINT);
		KeysBufferUAV[BufferIndex] = RHICmdList.CreateUnorderedAccessView(KeysBufferRHI[BufferIndex], PF_R32_UINT);
		CreateInfo.DebugName = TEXT("ValuesBuffer");
		ValuesBufferRHI[BufferIndex] = __CreateVertexBuffer(BufferSize, BUF_Static | BUF_ShaderResource | BUF_UnorderedAccess, CreateInfo);
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
		FMemory::Memcpy(Buffer, Keys.GetData(), BufferSize);
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
		if (SortedKeys[KeyIndex] != RefSortedKeys[KeyIndex] || SortedValues[KeyIndex] != RefSortedKeys[KeyIndex])
		{
			IncorrectKeyIndex = KeyIndex;
			bSucceeded = false;
			break;
		}
	}

	if (bSucceeded)
	{
		UE_LOG(LogTemp, Log, TEXT("GPU Sort Test (%d keys+values) succeeded."), TestSize);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("GPU Sort Test (%d keys+values) FAILED."), TestSize);

		if (bDebugSort)
		{
			const int32 FirstKeyIndex = FMath::Max<int32>(IncorrectKeyIndex - 8, 0);
			const int32 LastKeyIndex = FMath::Min<int32>(FirstKeyIndex + 1024, TestSize - 1);
			UE_LOG(LogTemp, Log, TEXT("       Input    : S.Keys   : S.Values : Ref Sorted Keys"));
			for (int32 KeyIndex = FirstKeyIndex; KeyIndex <= LastKeyIndex; ++KeyIndex)
			{
				UE_LOG(LogTemp, Log, TEXT("%04u : %08X : %08X : %08X : %08X%s"),
					KeyIndex,
					Keys[KeyIndex],
					SortedKeys[KeyIndex],
					SortedValues[KeyIndex],
					RefSortedKeys[KeyIndex],
					(KeyIndex == IncorrectKeyIndex) ? TEXT(" <----") : TEXT("")
				);
			}
		}
	}

	return bSucceeded;
}


FGaussianSplatBaseQuadRenderer::FGaussianSplatBaseQuadRenderer() :
	m_numSplats(0),
	m_maxSplats(0),
	m_currReconFrameIndex(0),
	m_currReconstructedNumSplats(0)
{

}

FGaussianSplatBaseQuadRenderer::~FGaussianSplatBaseQuadRenderer()
{
	Destroy();
}

void FGaussianSplatBaseQuadRenderer::Destroy()
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

	ReleaseRHIResources();

	m_numSplats = 0;
	m_maxSplats = 0;
	m_currReconFrameIndex = 0;
	m_currReconstructedNumSplats = 0;
}



void FGaussianSplatBaseQuadRenderer::ReserveRHIResources(uint32_t splatCount)
{
	if (m_maxSplats < splatCount)
	{
		ReleaseRHIResources();

		std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

		m_maxSplats = FMath::RoundUpToPowerOfTwo(splatCount);

		FRHICommandListBase& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		for (uint32_t i = 0; i < GPU_SORT_BUFFER_COUNT; ++i)
		{
			FRHIResourceCreateInfo sortKeyListCreateInfo(*FString::Printf(TEXT("SortKeyListBuffer%d"), i));
			FRHIResourceCreateInfo sortValueListCreateInfo(*FString::Printf(TEXT("SortValueListBuffer%d"), i));

			// Create Sort Key and Sort Value buffer as raw buffer
			m_sortKeyListBuffer[i] = __CreateVertexBuffer(
				sizeof(uint32_t) * m_maxSplats,
				BUF_ShaderResource | BUF_UnorderedAccess, // <- has to remove StructuredBuffer and ByteAddressBuffer
				sortKeyListCreateInfo
			);

			m_sortValueListBuffer[i] = __CreateVertexBuffer(
				sizeof(uint32_t) * m_maxSplats,
				BUF_ShaderResource | BUF_UnorderedAccess, // <- has to remove StructuredBuffer and ByteAddressBuffer
				sortValueListCreateInfo
			);

		}

		// "Final" copy buffer, as byte address buffer, because vertex factory cannot reads raw buffer(as they are treated like textures in some APIs)
		FRHIResourceCreateInfo sortValueFinalCreateInfo(TEXT("SortValueFinal"));
		m_sortValueFinalBuffer = __CreateVertexBuffer(
			sizeof(uint32_t) * m_maxSplats,
			BUF_ShaderResource | BUF_UnorderedAccess | BUF_ByteAddressBuffer,
			sortValueFinalCreateInfo
		);

		// Position buffer
		FRHIResourceCreateInfo EncodedSplatPositionCreateInfo(TEXT("EncodedSplatPositionBuffer"));
		m_encodedSplatPositionBuffer = RHICmdList.CreateBuffer(
			sizeof(EncodedSplatVector3) * m_maxSplats,
			BUF_ShaderResource |
			BUF_StructuredBuffer,
			sizeof(EncodedSplatVector3),
			ERHIAccess::SRVMask,
			EncodedSplatPositionCreateInfo
		);

		// Colour + alpha buffer
		FRHIResourceCreateInfo EncodedSplatColourAlphaCreateInfo(TEXT("EncodedSplatColourAlphaBuffer"));
		m_encodedSplatColourAlphaBuffer = RHICmdList.CreateBuffer(
			sizeof(EncodedSplatColourAlpha) * m_maxSplats,
			BUF_ShaderResource |
			BUF_StructuredBuffer,
			sizeof(EncodedSplatColourAlpha),
			ERHIAccess::SRVMask,
			EncodedSplatColourAlphaCreateInfo
		);

		// Scale buffer
		FRHIResourceCreateInfo EncodedSplatScaleCreationInfo(TEXT("EncodedSplatScaleBuffer"));
		m_encodedSplatScaleBuffer = RHICmdList.CreateBuffer(
			sizeof(EncodedSplatScale) * m_maxSplats,
			BUF_ShaderResource |
			BUF_StructuredBuffer,
			sizeof(EncodedSplatScale),
			ERHIAccess::SRVMask,
			EncodedSplatScaleCreationInfo
		);

		// Rotation buffer
		FRHIResourceCreateInfo EncodedSplatRotationCreationInfo(TEXT("EncodedSplatRotationBuffer"));
		m_encodedSplatRotationBuffer = RHICmdList.CreateBuffer(
			sizeof(EncodedSplatRotation) * m_maxSplats,
			BUF_ShaderResource |
			BUF_StructuredBuffer,
			sizeof(EncodedSplatRotation),
			ERHIAccess::SRVMask,
			EncodedSplatRotationCreationInfo
		);

		// SH Coeffs buffer
		FRHIResourceCreateInfo EncodedSplatSHCoeffsCreationInfo(TEXT("EncodedSplatSHCoeffsBuffer"));
		m_encodedSplatSHCoeffsBuffer = RHICmdList.CreateBuffer(
			sizeof(EncodedSplat3DegreeSHCoeffs) * m_maxSplats,
			BUF_ShaderResource |
			BUF_StructuredBuffer,
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
			// Create UAV and SRV like RunGPUSortTest()
			m_sortKeyListSRV[i] = __CreateSRV(m_sortKeyListBuffer[i], sizeof(uint32), PF_R32_UINT);
			m_sortKeyListUAV[i] = __CreateUAV(m_sortKeyListBuffer[i], PF_R32_UINT);
			m_sortValueListSRV[i] = __CreateSRV(m_sortValueListBuffer[i], sizeof(uint32), PF_R32_UINT);
			m_sortValueListUAV[i] = __CreateUAV(m_sortValueListBuffer[i], PF_R32_UINT);

		}

		m_sortValueFinalSRV = __CreateSRV(m_sortValueFinalBuffer, sizeof(uint32), PF_R32_UINT);
		m_sortValueFinalUAV = __CreateUAV(m_sortValueFinalBuffer, PF_R32_UINT);

		m_encodedSplatPositionSRV = __CreateSRV(m_encodedSplatPositionBuffer);
		m_encodedSplatColourAlphaSRV = __CreateSRV(m_encodedSplatColourAlphaBuffer);
		m_encodedSplatScaleSRV = __CreateSRV(m_encodedSplatScaleBuffer);
		m_encodedSplatRotationSRV = __CreateSRV(m_encodedSplatRotationBuffer);
		m_encodedSplatSHCoeffsSRV = __CreateSRV(m_encodedSplatSHCoeffsBuffer);

		m_splatViewUAV = __CreateUAV(m_splatViewBuffer, false, false);;
		m_splatViewSRV = __CreateSRV(m_splatViewBuffer);
	}
}


void FGaussianSplatBaseQuadRenderer::ReleaseRHIResources()
{
	std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

	m_sortValueFinalSRV.SafeRelease();
	m_sortValueFinalUAV.SafeRelease();

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

	m_sortValueFinalBuffer.SafeRelease();
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


void FGaussianSplatBaseQuadRenderer::RunPreprocessStage_RenderThread(FRHICommandListImmediate& RHICmdList, const FMatrix& ObjectToWorld, const FMatrix& InView, const FMatrix& InProj, const FVector& InCameraPositionWS,
	const FVector4& InScreenParam, bool bIsShadowPass, float splatDecimation, float extraSplatScale, float cov2DSqrtKernelSize, bool showSH0, bool showSH1, bool showSH2, bool showSH3, std::shared_ptr<const EvercoastGaussianSplatCSResult> InGaussianData)
{
	

	if (!InGaussianData)
		return;

	// If it's just in-between a ReleaseRHI and a InitRHI, then all the RHI resources may still missing a InitRHI() call
	// Normally this function will only be called within RHI thread. However it will be called on main thread when 
	// EvercoastGaussianSplatCSRendererComp::bReconstructOnTickOnly is set to true
	if (!m_encodedSplatPositionBuffer)
		return;


	std::lock_guard<std::recursive_mutex> guard(m_accessRHIMutex);

	uint32_t splatCount = InGaussianData->pointCount;
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	uint32 ThreadGroupCount;

	{
		//////////////////////// INIT SORT VALUE LIST
		// First initialize sorting data
		RHICmdList.Transition(FRHITransitionInfo(m_sortValueListUAV[0], ERHIAccess::Unknown, ERHIAccess::UAVMask));

		TShaderMapRef<FGaussianSplatInitSortDataCS> InitSortDataCS(ShaderMap);

		SetComputePipelineState(RHICmdList, InitSortDataCS.GetComputeShader());

		// Bind
		FGaussianSplatInitSortDataCS::FParameters ShaderParameters;
		ShaderParameters.SplatCount = splatCount;
		ShaderParameters.SortValueList_A = m_sortValueListUAV[0];
		SetShaderParameters(RHICmdList, InitSortDataCS, InitSortDataCS.GetComputeShader(), ShaderParameters);

		// Dispatch
		ThreadGroupCount = FMath::DivideAndRoundUp<uint32>(splatCount, INIT_SORTING_THREADS);
		RHICmdList.DispatchComputeShader(ThreadGroupCount, 1, 1);

		// Unbind
		UnsetShaderUAVs(RHICmdList, InitSortDataCS, InitSortDataCS.GetComputeShader());
	}

	/////////////////////////// PREPROCESS QUAD
	// Then decode & calculate splat view data
	// Upload position data
	void* PositionBufferData = RHICmdList.LockBuffer(m_encodedSplatPositionBuffer, 0, sizeof(EncodedSplatVector3) * splatCount, RLM_WriteOnly);
	FMemory::Memcpy(PositionBufferData, InGaussianData->packedPositions, InGaussianData->packedPositionsSize);
	RHICmdList.UnlockBuffer(m_encodedSplatPositionBuffer);

	// Upload colour & alpha data
	void* ColourAlphaBufferData = RHICmdList.LockBuffer(m_encodedSplatColourAlphaBuffer, 0, sizeof(EncodedSplatColourAlpha) * splatCount, RLM_WriteOnly);
	FMemory::Memcpy(ColourAlphaBufferData, InGaussianData->packedColourAlphas, InGaussianData->packedColourAlphasSize);
	RHICmdList.UnlockBuffer(m_encodedSplatColourAlphaBuffer);

	// Upload scale data
	void* ScaleBufferData = RHICmdList.LockBuffer(m_encodedSplatScaleBuffer, 0, sizeof(EncodedSplatScale) * splatCount, RLM_WriteOnly);
	FMemory::Memcpy(ScaleBufferData, InGaussianData->packedScales, InGaussianData->packedScalesSize);
	RHICmdList.UnlockBuffer(m_encodedSplatScaleBuffer);

	// Upload rotation data
	void* RotationBufferData = RHICmdList.LockBuffer(m_encodedSplatRotationBuffer, 0, sizeof(EncodedSplatRotation) * splatCount, RLM_WriteOnly);
	FMemory::Memcpy(RotationBufferData, InGaussianData->packedRotations, InGaussianData->packedRotationsSize);
	RHICmdList.UnlockBuffer(m_encodedSplatRotationBuffer);

	// Upload SH coeffs data
	void* SHCoeffsBufferData = RHICmdList.LockBuffer(m_encodedSplatSHCoeffsBuffer, 0, sizeof(EncodedSplat3DegreeSHCoeffs) * splatCount, RLM_WriteOnly);
	FMemory::Memcpy(SHCoeffsBufferData, InGaussianData->packedSHCoeffs, InGaussianData->packedSHCoeffsSize);
	RHICmdList.UnlockBuffer(m_encodedSplatSHCoeffsBuffer);


	RHICmdList.Transition(FRHITransitionInfo(m_encodedSplatPositionBuffer, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(m_encodedSplatColourAlphaBuffer, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(m_encodedSplatScaleBuffer, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(m_encodedSplatRotationBuffer, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(m_encodedSplatSHCoeffsBuffer, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(m_splatViewUAV, ERHIAccess::Unknown, ERHIAccess::UAVMask));
	RHICmdList.Transition(FRHITransitionInfo(m_sortKeyListUAV[0], ERHIAccess::Unknown, ERHIAccess::UAVMask));

	TShaderMapRef<FGaussianSplatQuadRendererPreprocessComputeShader> CalcSplatViewDataCS(ShaderMap);


	SetComputePipelineState(RHICmdList, CalcSplatViewDataCS.GetComputeShader());

	FMatrix WorldToObject = ObjectToWorld.Inverse();
	uint32_t shDim = (InGaussianData->shDegree + 1) * (InGaussianData->shDegree + 1) - 1;

	// Bind UAV
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	FRHIBatchedShaderParameters& BatchedShaderParams2 = RHICmdList.GetScratchShaderParameters();

	CalcSplatViewDataCS->SetupTransformsAndUniforms(BatchedShaderParams2, ObjectToWorld, WorldToObject,
		splatCount,
		InGaussianData->shDegree,
		shDim,
		InGaussianData->positionScalar,
		InView, InProj, InCameraPositionWS, InScreenParam, bIsShadowPass, splatDecimation, extraSplatScale, cov2DSqrtKernelSize,
		showSH0, showSH1, showSH2, showSH3, 1 /* sort from back to front */
	);
	CalcSplatViewDataCS->SetupIOBuffers(BatchedShaderParams2, m_encodedSplatPositionSRV, m_encodedSplatColourAlphaSRV, m_encodedSplatScaleSRV, m_encodedSplatRotationSRV, m_encodedSplatSHCoeffsSRV, m_splatViewUAV, m_sortKeyListUAV[0]);
	RHICmdList.SetBatchedShaderParameters(CalcSplatViewDataCS.GetComputeShader(), BatchedShaderParams2);
#else

		// Transform data only can get from SceneProxy::GetDynamicMeshElements, so this function is called with up-to-date view data
	CalcSplatViewDataCS->SetupTransformsAndUniforms(RHICmdList, ObjectToWorld, WorldToObject,
		splatCount,
		InGaussianData->shDegree,
		shDim,
		InGaussianData->positionScalar,
		InView, InProj, InCameraPositionWS, InScreenParam, bIsShadowPass, splatDecimation, extraSplatScale, cov2DSqrtKernelSize,
		showSH0, showSH1, showSH2, showSH3, 1 /* sort from back to front */
	);
	CalcSplatViewDataCS->SetupIOBuffers(RHICmdList, m_encodedSplatPositionSRV, m_encodedSplatColourAlphaSRV, m_encodedSplatScaleSRV, m_encodedSplatRotationSRV, m_encodedSplatSHCoeffsSRV, m_splatViewUAV, m_sortKeyListUAV[0]);
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

	/////////////////////////// GPU SORT
	// Quad renderer to perform GPU sort here, not to confused with the GPU sort in the second stage of the tile renderer.
	// GPU Radix sort, element count is different
	int resultBufferIndex;
	if (!bIsShadowPass)
	{


		// GPUSort shouldn handle transition resource itself
		FGPUSortBuffers SortBuffers; // fill in buffers
		SortBuffers.RemoteKeySRVs[0] = m_sortKeyListSRV[0];
		SortBuffers.RemoteKeySRVs[1] = m_sortKeyListSRV[1];
		SortBuffers.RemoteValueSRVs[0] = m_sortValueListSRV[0];
		SortBuffers.RemoteValueSRVs[1] = m_sortValueListSRV[1];

		SortBuffers.RemoteKeyUAVs[0] = m_sortKeyListUAV[0];
		SortBuffers.RemoteKeyUAVs[1] = m_sortKeyListUAV[1];
		SortBuffers.RemoteValueUAVs[0] = m_sortValueListUAV[0];
		SortBuffers.RemoteValueUAVs[1] = m_sortValueListUAV[1];


		// Run gpu sorter
		resultBufferIndex = SortGPUBuffers(RHICmdList, SortBuffers, 0, 0xFFFFFFFF, (int32)splatCount, GMaxRHIFeatureLevel);

		// Sort key list for VF shader to read
		if (resultBufferIndex == 0)
		{
			RHICmdList.Transition(FRHITransitionInfo(m_sortValueListUAV[0], ERHIAccess::UAVMask, ERHIAccess::SRVMask));
			RHICmdList.Transition(FRHITransitionInfo(m_sortKeyListUAV[0], ERHIAccess::UAVMask, ERHIAccess::SRVMask));
		}
		else
		{
			RHICmdList.Transition(FRHITransitionInfo(m_sortValueListUAV[1], ERHIAccess::UAVMask, ERHIAccess::SRVMask));
			RHICmdList.Transition(FRHITransitionInfo(m_sortKeyListUAV[1], ERHIAccess::UAVMask, ERHIAccess::SRVMask));
		}
	}
	else
	{
		resultBufferIndex = 0;
		RHICmdList.Transition(FRHITransitionInfo(m_sortValueListUAV[0], ERHIAccess::Unknown, ERHIAccess::SRVMask));
	}

	//////////////////////
	// Copy raw buffer to byte address buffer, because GPUSort on Mac doesn't like byte address buffer or structured buffer
	// And vertex factory doesn't like raw buffer
	{
		RHICmdList.Transition(FRHITransitionInfo(m_sortValueFinalUAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute));

		TShaderMapRef<FGaussianSplatCopySortDataCS> CopySortDataCS(ShaderMap);

		SetComputePipelineState(RHICmdList, CopySortDataCS.GetComputeShader());

		// Bind
		FGaussianSplatCopySortDataCS::FParameters ShaderParameters;
		ShaderParameters.SplatCount = splatCount;
		ShaderParameters.SortValueResult = m_sortValueListSRV[resultBufferIndex];
		ShaderParameters.SortValueFinal = m_sortValueFinalUAV;
		SetShaderParameters(RHICmdList, CopySortDataCS, CopySortDataCS.GetComputeShader(), ShaderParameters);

		// Dispatch
		ThreadGroupCount = FMath::DivideAndRoundUp<uint32>(splatCount, COPY_SORTING_THREADS);
		RHICmdList.DispatchComputeShader(ThreadGroupCount, 1, 1);

		// Unbind
		UnsetShaderUAVs(RHICmdList, CopySortDataCS, CopySortDataCS.GetComputeShader());
	}

	{
		std::lock_guard<std::recursive_mutex> guard2(m_accessMetadataMutex);

		m_currReconstructedNumSplats = splatCount;
		m_currReconFrameIndex = InGaussianData->frameIndex;
	}
	
}