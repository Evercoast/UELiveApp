#include "Gaussian/GaussianSplatOffscreenQuadRendererShader.h"
#include "DataDrivenShaderPlatformInfo.h"

TGlobalResource<FOffscreenGaussianQuadVertexDeclaration> GOffscreenGaussianQuadVertexDeclaration;
TGlobalResource<FOffscreenGaussianQuadVertexBuffer> GOffscreenGaussianQuadVertexBuffer;
TGlobalResource<FOffscreenGaussianQuadIndexBuffer> GOffscreenGaussianQuadIndexBuffer;

IMPLEMENT_SHADER_TYPE(, FOffscreenGaussianQuadVS, TEXT("/EvercoastShaders/EvercoastGaussianSplatOffscreenQuadRenderer.usf"), TEXT("OffscreenGaussianQuadVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FOffscreenGaussianQuadPS, TEXT("/EvercoastShaders/EvercoastGaussianSplatOffscreenQuadRenderer.usf"), TEXT("OffscreenGaussianQuadPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FOffscreenGaussianQuadBakeDepthPS, TEXT("/EvercoastShaders/EvercoastGaussianSplatOffscreenQuadRenderer.usf"), TEXT("OffscreenGaussianQuadBakeDepthPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FOffscreenGaussianQuadFastVS, TEXT("/EvercoastShaders/EvercoastGaussianSplatOffscreenQuadRenderer.usf"), TEXT("OffscreenGaussianQuadFastVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FOffscreenGaussianQuadFastPS, TEXT("/EvercoastShaders/EvercoastGaussianSplatOffscreenQuadRenderer.usf"), TEXT("OffscreenGaussianQuadFastPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FOffscreenGaussianQuadBakeDepthFastPS, TEXT("/EvercoastShaders/EvercoastGaussianSplatOffscreenQuadRenderer.usf"), TEXT("OffscreenGaussianQuadBakeDepthFastPS"), SF_Pixel);

IMPLEMENT_SHADER_TYPE(, FOffscreenGaussianQuadAlphaCutoutVS, TEXT("/EvercoastShaders/EvercoastGaussianSplatOffscreenQuadRenderer.usf"), TEXT("OffscreenGaussianQuadAlphaCutoutVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FOffscreenGaussianQuadAlphaCutoutPS, TEXT("/EvercoastShaders/EvercoastGaussianSplatOffscreenQuadRenderer.usf"), TEXT("OffscreenGaussianQuadAlphaCutoutPS"), SF_Pixel);
