#pragma once
#include "GlobalShader.h"
#include "RHIStaticStates.h"
#include "ShaderParameterUtils.h"
#include <memory>
#include "EvercoastLocalVoxelFrame.h"

class FWorldNormalGenVS : public FGlobalShader
{
    DECLARE_EXPORTED_SHADER_TYPE(FWorldNormalGenVS, Global, EVERCOASTPLAYBACK_API);

    LAYOUT_FIELD(FShaderParameter, ObjectToCameraTransform);
    LAYOUT_FIELD(FShaderParameter, ObjectToProjectionTransform);

    FWorldNormalGenVS() { }
    FWorldNormalGenVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FGlobalShader(Initializer)
    {
        ObjectToCameraTransform.Bind(Initializer.ParameterMap, TEXT("ObjectToCamera"), SPF_Optional);
        ObjectToProjectionTransform.Bind(Initializer.ParameterMap, TEXT("ObjectToProjection"), SPF_Optional);
    }

#if ENGINE_MAJOR_VERSION >= 5
    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Params, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Params, OutEnvironment);
    }
#else
    static void ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Params, OutEnvironment);
    }
#endif

    static bool ShouldCompilePermutation(const FShaderPermutationParameters& Params)
    {
        return true;
    }

public:
    void SetTransforms(FRHICommandList& RHICmdList, const FMatrix& objectToCamera, const FMatrix& objectToProjection)
    {
#if ENGINE_MAJOR_VERSION >= 5
        FMatrix44f objectToCamera44f = ToMatrix44f(objectToCamera);
        FMatrix44f objectToProjection44f = ToMatrix44f(objectToProjection);
#if ENGINE_MINOR_VERSION >= 3
        FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();

        SetShaderValue(BatchedParameters, ObjectToCameraTransform, objectToCamera44f, 0);
        SetShaderValue(BatchedParameters, ObjectToProjectionTransform, objectToProjection44f, 0);
        RHICmdList.SetBatchedShaderParameters(RHICmdList.GetBoundVertexShader(), BatchedParameters);

#else
        SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), ObjectToCameraTransform, objectToCamera44f);
        SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), ObjectToProjectionTransform, objectToProjection44f);
#endif
#else
        SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), ObjectToCameraTransform, objectToCamera);
        SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), ObjectToProjectionTransform, objectToProjection);
#endif
    }
};


class FWorldNormalGenPS : public FGlobalShader
{
    DECLARE_EXPORTED_SHADER_TYPE(FWorldNormalGenPS, Global, EVERCOASTPLAYBACK_API);

    LAYOUT_FIELD(FShaderParameter, CameraToWorldTransform);

    FWorldNormalGenPS() { }
    FWorldNormalGenPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FGlobalShader(Initializer)
    {
        CameraToWorldTransform.Bind(Initializer.ParameterMap, TEXT("CameraToWorld"), SPF_Optional);
    }

#if ENGINE_MAJOR_VERSION >= 5
    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Params, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Params, OutEnvironment);
    }
#else
    static void ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Params, OutEnvironment);
    }
#endif

    static bool ShouldCompilePermutation(const FShaderPermutationParameters& Params)
    {
        return true;
    }
public:
    void SetTransforms(FRHICommandList& RHICmdList, const FMatrix& cameraToWorld)
    {
#if ENGINE_MAJOR_VERSION >= 5
        FMatrix44f cameraToWorld44f = ToMatrix44f(cameraToWorld);
#if ENGINE_MINOR_VERSION >= 3
        FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();
        SetShaderValue(BatchedParameters, CameraToWorldTransform, cameraToWorld44f, 0);
        RHICmdList.SetBatchedShaderParameters(RHICmdList.GetBoundPixelShader(), BatchedParameters);

#else
        SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(), CameraToWorldTransform, cameraToWorld44f);
#endif
#else
        SetShaderValue(RHICmdList, RHICmdList.GetBoundPixelShader(), CameraToWorldTransform, cameraToWorld);
#endif
    }
};



class FVoxelWorldNormalGenVS : public FWorldNormalGenVS
{
    DECLARE_EXPORTED_SHADER_TYPE(FVoxelWorldNormalGenVS, Global, EVERCOASTPLAYBACK_API);

    LAYOUT_FIELD(FShaderParameter, Evercoast_Bounds_Min);
    LAYOUT_FIELD(FShaderParameter, Evercoast_Bounds_Dim);
    LAYOUT_FIELD(FShaderParameter, Evercoast_Position_Rescale);
    LAYOUT_FIELD(FShaderParameter, Evercoast_ToUnrealUnit);
    LAYOUT_FIELD(FShaderResourceParameter, Evercoast_Positions_SRV);

    FVoxelWorldNormalGenVS() { }
    FVoxelWorldNormalGenVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FWorldNormalGenVS(Initializer)
    {
        Evercoast_Bounds_Min.Bind(Initializer.ParameterMap, TEXT("Evercoast_Bounds_Min"), SPF_Mandatory);
        Evercoast_Bounds_Dim.Bind(Initializer.ParameterMap, TEXT("Evercoast_Bounds_Dim"), SPF_Mandatory);
        Evercoast_Position_Rescale.Bind(Initializer.ParameterMap, TEXT("Evercoast_Position_Rescale"), SPF_Mandatory);
        Evercoast_ToUnrealUnit.Bind(Initializer.ParameterMap, TEXT("Evercoast_ToUnrealUnit"), SPF_Mandatory);
        Evercoast_Positions_SRV.Bind(Initializer.ParameterMap, TEXT("Evercoast_Positions"), SPF_Mandatory);
    }

#if ENGINE_MAJOR_VERSION >= 5
    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Params, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Params, OutEnvironment);
    }
#else
    static void ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Params, OutEnvironment);
    }
#endif

    static bool ShouldCompilePermutation(const FShaderPermutationParameters& Params)
    {
        return true;
    }

public:
    void SetInstancingData(FRHICommandList& RHICmdList, const FVector3f& boundsMin, float boundsDim, float positionRescale, float toUnrealUnit, FShaderResourceViewRHIRef positionSRV)
    {
        auto shader = RHICmdList.GetBoundVertexShader();
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
        FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();

        SetShaderValue(BatchedParameters, Evercoast_Bounds_Min, boundsMin, 0);
        SetShaderValue(BatchedParameters, Evercoast_Bounds_Dim, boundsDim, 0);
        SetShaderValue(BatchedParameters, Evercoast_Position_Rescale, positionRescale, 0);
        SetShaderValue(BatchedParameters, Evercoast_ToUnrealUnit, 100.0f, 0);
        SetSRVParameter(BatchedParameters, Evercoast_Positions_SRV, positionSRV);
        RHICmdList.SetBatchedShaderParameters(shader, BatchedParameters);
#else
        SetShaderValue(RHICmdList, shader, Evercoast_Bounds_Min, boundsMin);
        SetShaderValue(RHICmdList, shader, Evercoast_Bounds_Dim, boundsDim);
        SetShaderValue(RHICmdList, shader, Evercoast_Position_Rescale, positionRescale);
        SetShaderValue(RHICmdList, shader, Evercoast_ToUnrealUnit, 100.0f);
        SetSRVParameter(RHICmdList, shader, Evercoast_Positions_SRV, positionSRV);
#endif
    }

    void SetTransforms(FRHICommandList& RHICmdList, const FMatrix& objectToCamera, const FMatrix& objectToProjection)
    {
#if ENGINE_MAJOR_VERSION >= 5
        FMatrix44f objectToCamera44f = ToMatrix44f(objectToCamera);
        FMatrix44f objectToProjection44f = ToMatrix44f(objectToProjection);

#if ENGINE_MINOR_VERSION >= 3
        FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();
        SetShaderValue(BatchedParameters, ObjectToCameraTransform, objectToCamera44f, 0);
        SetShaderValue(BatchedParameters, ObjectToProjectionTransform, objectToProjection44f, 0);
        RHICmdList.SetBatchedShaderParameters(RHICmdList.GetBoundVertexShader(), BatchedParameters);
#else
        SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), ObjectToCameraTransform, objectToCamera44f);
        SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), ObjectToProjectionTransform, objectToProjection44f);
#endif
#else
        SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), ObjectToCameraTransform, objectToCamera);
        SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), ObjectToProjectionTransform, objectToProjection);
#endif
    }
};


class FVoxelWorldNormalGenPS : public FWorldNormalGenPS
{
    DECLARE_EXPORTED_SHADER_TYPE(FVoxelWorldNormalGenPS, Global, EVERCOASTPLAYBACK_API);

    FVoxelWorldNormalGenPS() { }
    FVoxelWorldNormalGenPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
        : FWorldNormalGenPS(Initializer)
    {
    }

};