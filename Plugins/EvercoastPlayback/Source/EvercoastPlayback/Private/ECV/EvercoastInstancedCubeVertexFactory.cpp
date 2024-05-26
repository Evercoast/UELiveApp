#include "EvercoastInstancedCubeVertexFactory.h"
#include "ShaderCompilerCore.h"
#include "MeshDrawShaderBindings.h"
#include "RenderUtils.h"
#include "MeshMaterialShader.h"
#include "EvercoastVoxelSceneProxy.h"
#include "EvercoastDecoder.h"
#include "EvercoastLocalVoxelFrame.h"
#include "MaterialShared.h"
#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 2
#include "MaterialDomain.h"
#endif
#endif


class FEvercoastInstancedCubeVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FEvercoastInstancedCubeVertexFactoryShaderParameters, NonVirtual);
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		/* We bind our shader paramters to the paramtermap that will be used with it, the SPF_Optional flags tells the compiler that this paramter is optional*/
		/* Otherwise, the shader compiler will complain when this parameter is not present in the shader file*/

		BoundsMin.Bind(ParameterMap, TEXT("Evercoast_Bounds_Min"), SPF_Optional);
		BoundsDim.Bind(ParameterMap, TEXT("Evercoast_Bounds_Dim"), SPF_Optional);
		PosRescale.Bind(ParameterMap, TEXT("Evercoast_Position_Rescale"), SPF_Optional);
		PositionsSRV.Bind(ParameterMap, TEXT("Evercoast_Positions"), SPF_Optional);
		ColoursSRV.Bind(ParameterMap, TEXT("Evercoast_Colours"), SPF_Optional);
		ToUnrealUnit.Bind(ParameterMap, TEXT("Evercoast_ToUnrealUnit"), SPF_Optional);
	};

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
		const FEvercoastInstancedCubeVertexFactory* InstancedCubeVertexFactory = ((const FEvercoastInstancedCubeVertexFactory*)VertexFactory);

		FRHIUniformBuffer* VertexFactoryUniformBuffer = static_cast<FRHIUniformBuffer*>(BatchElement.VertexFactoryUserData);

		if (InstancedCubeVertexFactory->SupportsManualVertexFetch(FeatureLevel) || UseGPUScene(GMaxRHIShaderPlatform, FeatureLevel))
		{
			if (!VertexFactoryUniformBuffer)
			{
				// No batch element override
				VertexFactoryUniformBuffer = InstancedCubeVertexFactory->GetUniformBuffer();
			}

			ShaderBindings.Add(Shader->GetUniformBufferParameter<FLocalVertexFactoryUniformShaderParameters>(), VertexFactoryUniformBuffer);
		}
		
		InstancedCubeVertexFactory->SceneProxy->LockVoxelData();

		auto voxelFrame = InstancedCubeVertexFactory->GetInstancingData();
		ShaderBindings.Add(BoundsMin, voxelFrame->m_boundsMin);
		ShaderBindings.Add(BoundsDim, voxelFrame->m_boundsDim);

		float positionRescaleFactor = 1.0f / ((1 << voxelFrame->m_bitsPerVoxel) - 1);
		ShaderBindings.Add(PosRescale, positionRescaleFactor);

		ShaderBindings.Add(ToUnrealUnit, 100.0f);
		ShaderBindings.Add(PositionsSRV, InstancedCubeVertexFactory->m_positionsSRV);
		ShaderBindings.Add(ColoursSRV, InstancedCubeVertexFactory->m_coloursSRV);

		InstancedCubeVertexFactory->SceneProxy->UnlockVoxelData();
	};
private:
	
	LAYOUT_FIELD(FShaderParameter, BoundsMin);
	LAYOUT_FIELD(FShaderParameter, BoundsDim);
	LAYOUT_FIELD(FShaderParameter, PosRescale);
	LAYOUT_FIELD(FShaderParameter, ToUnrealUnit);
	LAYOUT_FIELD(FShaderResourceParameter, PositionsSRV);
	LAYOUT_FIELD(FShaderResourceParameter, ColoursSRV);
};

/*
class FEvercoastInstancedCubeVertexFactoryShaderParametersPS : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FEvercoastInstancedCubeVertexFactoryShaderParametersPS, NonVirtual);
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		ColoursSRV.Bind(ParameterMap, TEXT("Evercoast_Colours"), SPF_Optional);
	};

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
		const FEvercoastInstancedCubeVertexFactory* InstancedCubeVertexFactory = ((const FEvercoastInstancedCubeVertexFactory*)VertexFactory);

		FRHIUniformBuffer* VertexFactoryUniformBuffer = static_cast<FRHIUniformBuffer*>(BatchElement.VertexFactoryUserData);

		if (InstancedCubeVertexFactory->SupportsManualVertexFetch(FeatureLevel) || UseGPUScene(GMaxRHIShaderPlatform, FeatureLevel))
		{
			if (!VertexFactoryUniformBuffer)
			{
				// No batch element override
				VertexFactoryUniformBuffer = InstancedCubeVertexFactory->GetUniformBuffer();
			}

			ShaderBindings.Add(Shader->GetUniformBufferParameter<FLocalVertexFactoryUniformShaderParameters>(), VertexFactoryUniformBuffer);
		}

		ShaderBindings.Add(ColoursSRV, InstancedCubeVertexFactory->SceneProxy->GetVoxelColoursSRV());
	};
private:
	LAYOUT_FIELD(FShaderResourceParameter, ColoursSRV);
};
*/

/*
// ~Beginning of Global shader uniform struct
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FEvercoastInstancedCubeVertexFactoryUniformShaderParameters, )
	SHADER_PARAMETER(uint32, TransformIndex)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FEvercoastInstancedCubeVertexFactoryUniformShaderParameters, "EvercoastVF");
// ~End of Global shader uniform struct
*/

IMPLEMENT_TYPE_LAYOUT(FEvercoastInstancedCubeVertexFactoryShaderParameters);
//IMPLEMENT_TYPE_LAYOUT(FEvercoastInstancedCubeVertexFactoryShaderParametersPS);

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FEvercoastInstancedCubeVertexFactory, SF_Vertex, FEvercoastInstancedCubeVertexFactoryShaderParameters);
//IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FEvercoastInstancedCubeVertexFactory, SF_Pixel, FEvercoastInstancedCubeVertexFactoryShaderParametersPS);

#if ENGINE_MAJOR_VERSION >= 5
#if ENGINE_MINOR_VERSION >= 1

IMPLEMENT_VERTEX_FACTORY_TYPE(FEvercoastInstancedCubeVertexFactory, "/EvercoastShaders/EvercoastLocalVertexFactory.ush",
	EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPrecisePrevWorldPos
	| EVertexFactoryFlags::SupportsPositionOnly
);
#else

IMPLEMENT_VERTEX_FACTORY_TYPE(FEvercoastInstancedCubeVertexFactory, "/EvercoastShaders/EvercoastLocalVertexFactory_5_0.ush",
	EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPrecisePrevWorldPos
	| EVertexFactoryFlags::SupportsPositionOnly
);

#endif
#else
IMPLEMENT_VERTEX_FACTORY_TYPE_EX(FEvercoastInstancedCubeVertexFactory, "/EvercoastShaders/EvercoastLocalVertexFactory_4_27.ush",
	true, false, true, true, true, false, false);
#endif

constexpr uint32_t VOXEL_COUNT_LIMIT = DECODER_MAX_VOXEL_COUNT;

FEvercoastInstancedCubeVertexFactory::FEvercoastInstancedCubeVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const char* InDebugName) :
	FLocalVertexFactory(InFeatureLevel, InDebugName)
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
	// VertexFactoryType should automatically disable support manual vertex fetch based on EVertexFactoryFlags
#else
	bSupportsManualVertexFetch = false;
#endif

	m_positions.AddZeroed(VOXEL_COUNT_LIMIT);
	m_colours.AddZeroed(VOXEL_COUNT_LIMIT);
}

bool FEvercoastInstancedCubeVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	if (Parameters.MaterialParameters.MaterialDomain == MD_Surface ||
		Parameters.MaterialParameters.bIsDefaultMaterial)
		return true;
	return false;
}

void FEvercoastInstancedCubeVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
	const bool ContainsManualVertexFetch = OutEnvironment.HasCompileArgument(TEXT("MANUAL_VERTEX_FETCH"));
#else
	const bool ContainsManualVertexFetch = OutEnvironment.GetDefinitions().Contains("MANUAL_VERTEX_FETCH");
#endif
	if (!ContainsManualVertexFetch)
	{
		OutEnvironment.SetDefine(TEXT("MANUAL_VERTEX_FETCH"), TEXT("0"));
	}
	// Note: we have to exclude the shader uniform specific to instanced cube because other vertex factories are sharing the same shader file
	// Failing to do so will render a binding error or invalid layout on startup
	OutEnvironment.SetDefine(TEXT("EVERCOAST_INSTANCEDCUBE"), TEXT("1"));
	// https://docs.unrealengine.com/4.27/en-US/ProgrammingAndScripting/Rendering/ShaderDevelopment/
	OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
void FEvercoastInstancedCubeVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
{
	FLocalVertexFactory::InitRHI(RHICmdList);
#else
void FEvercoastInstancedCubeVertexFactory::InitRHI()
{
	FLocalVertexFactory::InitRHI();
#endif

	// Positions
	//We first create a resource array to use it in the create info for initializing the structured buffer on creation
	TResourceArray<uint64_t>* ResourceArray = new TResourceArray<uint64_t>(true);
	//Set the debug name so we can find the resource when debugging in RenderDoc
	FRHIResourceCreateInfo CreateInfo(TEXT("Evercoast_PositionsTex"));
	ResourceArray->Append(m_positions);
	CreateInfo.ResourceArray = ResourceArray;

#if ENGINE_MAJOR_VERSION >= 5
#if ENGINE_MINOR_VERSION >= 1
	const FRHITextureCreateDesc posTexDesc =
		FRHITextureCreateDesc::Create2D(CreateInfo.DebugName, 2048, 2048, EPixelFormat::PF_R32G32_UINT)
		.SetNumMips(1).SetNumSamples(1)
		.SetFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::Dynamic);
	m_positionsTex = RHICreateTexture(posTexDesc);
#else
	m_positionsTex = RHICreateTexture2D(2048, 2048,
		EPixelFormat::PF_R32G32_UINT, 1, 1,
		ETextureCreateFlags::ShaderResource | ETextureCreateFlags::Dynamic,
		CreateInfo);
#endif
#else
	m_positionsTex = RHICreateTexture2D(2048, 2048,
		EPixelFormat::PF_R32G32_UINT, 1, 1,
		ETextureCreateFlags::TexCreate_ShaderResource | ETextureCreateFlags::TexCreate_Dynamic,
		CreateInfo);
#endif

	///////////////////////////////////////////////////////////////
	//// CREATING AN SRV FOR THE TEXTURE SO WA CAN USE IT AS A SHADER RESOURCE PARAMETER AND BIND IT TO THE VERTEX FACTORY
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	m_positionsSRV = RHICmdList.CreateShaderResourceView(m_positionsTex, 0);
#else
	m_positionsSRV = RHICreateShaderResourceView(m_positionsTex, 0);
#endif
	// Colours
	TResourceArray<uint32_t>* ColourResourceArray = new TResourceArray<uint32_t>(true);
	//Set the debug name so we can find the resource when debugging in RenderDoc
	FRHIResourceCreateInfo ColourCreateInfo(TEXT("Evercoast_ColoursTex"));
	ColourResourceArray->Append(m_colours);
	ColourCreateInfo.ResourceArray = ColourResourceArray;
	
#if ENGINE_MAJOR_VERSION >= 5
#if ENGINE_MINOR_VERSION >= 1
	const FRHITextureCreateDesc colourTexDesc = 
		FRHITextureCreateDesc::Create2D(ColourCreateInfo.DebugName, 2048, 2048, EPixelFormat::PF_R32_UINT)
		.SetNumMips(1).SetNumSamples(1)
		.SetFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::Dynamic);
	m_coloursTex = RHICreateTexture(colourTexDesc);
#else
	m_coloursTex = RHICreateTexture2D(2048, 2048,
		EPixelFormat::PF_R32_UINT, 1, 1,
		ETextureCreateFlags::ShaderResource | ETextureCreateFlags::Dynamic,
		CreateInfo);
#endif
#else
	m_coloursTex = RHICreateTexture2D(2048, 2048,
		EPixelFormat::PF_R32_UINT, 1, 1,
		ETextureCreateFlags::TexCreate_ShaderResource | ETextureCreateFlags::TexCreate_Dynamic,
		CreateInfo);
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
	m_coloursSRV = RHICmdList.CreateShaderResourceView(m_coloursTex, 0);
#else
	m_coloursSRV = RHICreateShaderResourceView(m_coloursTex, 0);
#endif
}

void FEvercoastInstancedCubeVertexFactory::ReleaseRHI()
{
	m_positionsSRV.SafeRelease();
	m_positionsTex.SafeRelease();

	m_coloursSRV.SafeRelease();
	m_coloursTex.SafeRelease();


	FLocalVertexFactory::ReleaseRHI();
}

void FEvercoastInstancedCubeVertexFactory::SetInstancingData(std::shared_ptr<EvercoastLocalVoxelFrame> voxelFrame)
{
	m_voxelFrame = voxelFrame;

	//Update the structured buffer only if it needs update
	if (m_positionsTex && m_coloursTex && m_voxelFrame)
	{
		uint32_t voxelCount = std::min(VOXEL_COUNT_LIMIT, m_voxelFrame->m_voxelCount);
		uint32_t stride = 0;
		void* LockedTexData = RHILockTexture2D(m_positionsTex, 0, EResourceLockMode::RLM_WriteOnly, stride, false);
		check(stride == 2048 * DECODER_POSITION_ELEMENT_SIZE);
		FMemory::Memcpy(LockedTexData, 
			m_voxelFrame->m_positionData, 
			voxelCount * DECODER_POSITION_ELEMENT_SIZE);
		RHIUnlockTexture2D(m_positionsTex, 0, false);

		LockedTexData = RHILockTexture2D(m_coloursTex, 0, EResourceLockMode::RLM_WriteOnly, stride, false);
		check(stride == 2048 * DECODER_COLOUR_ELEMENT_SIZE);
		FMemory::Memcpy(LockedTexData, 
			m_voxelFrame->m_colourData, 
			voxelCount * DECODER_COLOUR_ELEMENT_SIZE);
		RHIUnlockTexture2D(m_coloursTex, 0, false);
	}
}

std::shared_ptr<EvercoastLocalVoxelFrame> FEvercoastInstancedCubeVertexFactory::GetInstancingData() const
{
	return m_voxelFrame;
}