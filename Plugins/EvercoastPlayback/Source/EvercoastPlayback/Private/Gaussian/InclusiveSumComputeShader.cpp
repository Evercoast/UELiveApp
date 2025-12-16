#include "Gaussian/InclusiveSumComputeShader.h"
#include "DataDrivenShaderPlatformInfo.h"

#if PLATFORM_WINDOWS
IMPLEMENT_SHADER_TYPE(, FInclusiveSumComputeShader, TEXT("/EvercoastShaders/InclusiveSum.usf"), TEXT("CSInclusiveSum"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FInclusiveGroupSumComputeShader, TEXT("/EvercoastShaders/InclusiveSum.usf"), TEXT("CSInclusiveGroupSum"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FInclusiveSumAddGroupOffsetComputeShader, TEXT("/EvercoastShaders/InclusiveSum.usf"), TEXT("CSAddGroupOffsets"), SF_Compute);
#endif