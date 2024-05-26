#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5

inline FMatrix44f ToMatrix44f(const FMatrix& mat)
{
	FMatrix44f out;
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			out.M[i][j] = (float)(mat.M[i][j]);

	return out;
}

#elif ENGINE_MAJOR_VERSION == 4
#define FVector3f FVector
#define FVector2f FVector2D
#define FVector4f FVector4
#define FBoxSphereBounds3f FBoxSphereBounds
#define FSphere3f FSphere
#define FMatrix44f FMatrix
#else
#error Unknown Unreal Engine version!
#endif
