#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "UnrealEngineCompatibility.h"
#include "RenderGraphUtils.h"

#if PLATFORM_WINDOWS

class FInclusiveSumComputeShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FInclusiveSumComputeShader, Global)
	SHADER_USE_PARAMETER_STRUCT(FInclusiveSumComputeShader, FGlobalShader)
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32_t, io_size)
		SHADER_PARAMETER_SRV(StructuredBuffer<uint32_t>, input)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<uint32_t>, output)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<uint32_t>, groupSumsOutput)
	END_SHADER_PARAMETER_STRUCT()
};

class FInclusiveGroupSumComputeShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FInclusiveGroupSumComputeShader, Global)
	SHADER_USE_PARAMETER_STRUCT(FInclusiveGroupSumComputeShader, FGlobalShader)
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32_t, numGroups)
		SHADER_PARAMETER_SRV(StructuredBuffer<uint32_t>, groupSumsInput)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<uint32_t>, groupSumsScanOutput)
	END_SHADER_PARAMETER_STRUCT()
};


class FInclusiveSumAddGroupOffsetComputeShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FInclusiveSumAddGroupOffsetComputeShader, Global)
	SHADER_USE_PARAMETER_STRUCT(FInclusiveSumAddGroupOffsetComputeShader, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32_t, io_size)
		SHADER_PARAMETER_SRV(StructuredBuffer<uint32_t>, groupSumsScanInput)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<uint32_t>, finalOutput)
	END_SHADER_PARAMETER_STRUCT()
};
#endif