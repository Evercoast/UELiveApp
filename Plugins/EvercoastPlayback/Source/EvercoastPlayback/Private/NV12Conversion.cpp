#include "NV12Conversion.h"

TGlobalResource<FNV12TextureConversionVertexDeclaration> GNV12TextureConversionVertexDeclaration;
TGlobalResource<FNV12ConversionDummyIndexBuffer> GNV12TextureConversionIndexBuffer;
TGlobalResource<FNV12ConversionDummyVertexBuffer> GNV12TextureConversionVertexBuffer;

IMPLEMENT_SHADER_TYPE(, FNV12TextureConversionVS, TEXT("/EvercoastShaders/NV12TextureConversion.usf"), TEXT("MainVS"), SF_Vertex)
IMPLEMENT_SHADER_TYPE(, FNV12TextureConversionPS, TEXT("/EvercoastShaders/NV12TextureConversion.usf"), TEXT("MainPS"), SF_Pixel)
