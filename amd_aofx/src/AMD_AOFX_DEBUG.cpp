#include <d3d11.h>

#if AMD_AOFX_COMPILE_DYNAMIC_LIB
# define AMD_DLL_EXPORTS
#endif
#include "AMD_AOFX.h"

#if defined(AMD_AOFX_DEBUG)

#define _USE_MATH_DEFINES
#include <cmath>
#include <string>
#include <iostream>
#include <fstream>

#include "AMD_AOFX_OPAQUE.h"

#pragma warning( disable : 4996 ) // disable "stdio is deprecated" warnings

namespace AMD
{

AMD_AOFX_DLL_API AOFX_RETURN_CODE AOFX_DebugSerialize(AOFX_Desc& desc, const char* params)
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME " \n");

    HRESULT hr = S_OK;
    ID3D11Texture2D *pDepthT2D = NULL, *pNormalT2D = NULL;

    std::string strParams(params);
    std::wstring wstrParams(strParams.begin(), strParams.end());

    if (desc.m_pDepthSRV)
    {
        std::wstring depth = wstrParams + L".depth.dds";
        desc.m_pDepthSRV->GetResource((ID3D11Resource**)&pDepthT2D);
        hr = DirectX::SaveDDSTextureToFile(desc.m_pDeviceContext, pDepthT2D, depth.c_str());
        AMD_SAFE_RELEASE(pDepthT2D);
        if (hr != S_OK) 
        {
            AMD_OUTPUT_DEBUG_STRING("AMD_AO DebugSerialize Error : Can't save Depth Texture\n");
            return AOFX_RETURN_CODE_FAIL;
        }
    }
    if (desc.m_pNormalSRV)
    {
        std::wstring normal = wstrParams + L".normal.dds";
        desc.m_pNormalSRV->GetResource((ID3D11Resource**)&pNormalT2D);
        hr = DirectX::SaveDDSTextureToFile(desc.m_pDeviceContext, pNormalT2D, normal.c_str());
        AMD_SAFE_RELEASE(pNormalT2D);
        if (hr != S_OK) 
        {
            AMD_OUTPUT_DEBUG_STRING("AMD_AO DebugSerialize Error : Can't save Normal Texture\n");
            return AOFX_RETURN_CODE_FAIL;
        }
    }

    strParams += ".txt";
    FILE * file = fopen(strParams.c_str(), "wt");
    if (file != NULL)
    {
        serialize_uint(file, "desc.m_MultiResLayerCount", (uint *)&desc.m_MultiResLayerCount);

        for (uint i = 0; i < desc.m_MultiResLayerCount; i++)
        {
            serialize_uint(file, "desc.m_LayerProcess", (uint *)&desc.m_LayerProcess[i]);
            serialize_uint(file, "desc.m_BilateralBlurRadius", (uint *)&desc.m_BilateralBlurRadius[i]);
            serialize_uint(file, "desc.m_SampleCount", (uint *)&desc.m_SampleCount[i]);
            serialize_uint(file, "desc.m_NormalOption", (uint *)&desc.m_NormalOption[i]);
            serialize_uint(file, "desc.m_TapType", (uint *)&desc.m_TapType[i]);
            serialize_float(file, "desc.m_MultiResLayerScale", (float *)&desc.m_MultiResLayerScale[i]);
            serialize_float(file, "desc.m_PowIntensity", (float *)&desc.m_PowIntensity[i]);
            serialize_float(file, "desc.m_RejectRadius", (float *)&desc.m_RejectRadius[i]);
            serialize_float(file, "desc.m_AcceptRadius", (float *)&desc.m_AcceptRadius[i]);
            serialize_float(file, "desc.m_RecipFadeOutDist", (float *)&desc.m_RecipFadeOutDist[i]);
            serialize_float(file, "desc.m_LinearIntensity", (float *)&desc.m_LinearIntensity[i]);
            serialize_float(file, "desc.m_NormalScale", (float *)&desc.m_NormalScale[i]);
            serialize_float(file, "desc.m_ViewDistanceDiscard", (float *)&desc.m_ViewDistanceDiscard[i]);
            serialize_float(file, "desc.m_ViewDistanceFade", (float *)&desc.m_ViewDistanceFade[i]);
            serialize_float(file, "desc.m_DepthUpsampleThreshold", (float *)&desc.m_DepthUpsampleThreshold[i]);
        }

        serialize_uint(file, "desc.m_Implementation", (uint *)&desc.m_Implementation);

        serialize_float4(file, "desc.m_Camera.m_View.r[0]", (float *)&desc.m_Camera.m_View.r[0]);
        serialize_float4(file, "desc.m_Camera.m_View.r[1]", (float *)&desc.m_Camera.m_View.r[1]);
        serialize_float4(file, "desc.m_Camera.m_View.r[2]", (float *)&desc.m_Camera.m_View.r[2]);
        serialize_float4(file, "desc.m_Camera.m_View.r[3]", (float *)&desc.m_Camera.m_View.r[3]);

        serialize_float4(file, "desc.m_Camera.m_Projection.r[0]", (float *)&desc.m_Camera.m_Projection.r[0]);
        serialize_float4(file, "desc.m_Camera.m_Projection.r[1]", (float *)&desc.m_Camera.m_Projection.r[1]);
        serialize_float4(file, "desc.m_Camera.m_Projection.r[2]", (float *)&desc.m_Camera.m_Projection.r[2]);
        serialize_float4(file, "desc.m_Camera.m_Projection.r[3]", (float *)&desc.m_Camera.m_Projection.r[3]);

        serialize_float4(file, "desc.m_Camera.m_ViewProjection.r[0]", (float *)&desc.m_Camera.m_ViewProjection.r[0]);
        serialize_float4(file, "desc.m_Camera.m_ViewProjection.r[1]", (float *)&desc.m_Camera.m_ViewProjection.r[1]);
        serialize_float4(file, "desc.m_Camera.m_ViewProjection.r[2]", (float *)&desc.m_Camera.m_ViewProjection.r[2]);
        serialize_float4(file, "desc.m_Camera.m_ViewProjection.r[3]", (float *)&desc.m_Camera.m_ViewProjection.r[3]);

        serialize_float4(file, "desc.m_Camera.m_View_Inv.r[0]", (float *)&desc.m_Camera.m_View_Inv.r[0]);
        serialize_float4(file, "desc.m_Camera.m_View_Inv.r[1]", (float *)&desc.m_Camera.m_View_Inv.r[1]);
        serialize_float4(file, "desc.m_Camera.m_View_Inv.r[2]", (float *)&desc.m_Camera.m_View_Inv.r[2]);
        serialize_float4(file, "desc.m_Camera.m_View_Inv.r[3]", (float *)&desc.m_Camera.m_View_Inv.r[3]);

        serialize_float4(file, "desc.m_Camera.m_Projection_Inv.r[0]", (float *)&desc.m_Camera.m_Projection_Inv.r[0]);
        serialize_float4(file, "desc.m_Camera.m_Projection_Inv.r[1]", (float *)&desc.m_Camera.m_Projection_Inv.r[1]);
        serialize_float4(file, "desc.m_Camera.m_Projection_Inv.r[2]", (float *)&desc.m_Camera.m_Projection_Inv.r[2]);
        serialize_float4(file, "desc.m_Camera.m_Projection_Inv.r[3]", (float *)&desc.m_Camera.m_Projection_Inv.r[3]);

        serialize_float4(file, "desc.m_Camera.m_ViewProjection_Inv.r[0]", (float *)&desc.m_Camera.m_ViewProjection_Inv.r[0]);
        serialize_float4(file, "desc.m_Camera.m_ViewProjection_Inv.r[1]", (float *)&desc.m_Camera.m_ViewProjection_Inv.r[1]);
        serialize_float4(file, "desc.m_Camera.m_ViewProjection_Inv.r[2]", (float *)&desc.m_Camera.m_ViewProjection_Inv.r[2]);
        serialize_float4(file, "desc.m_Camera.m_ViewProjection_Inv.r[3]", (float *)&desc.m_Camera.m_ViewProjection_Inv.r[3]);

        serialize_float3(file, "desc.m_Camera.m_Position", (float *)&desc.m_Camera.m_Position);
        serialize_float3(file, "desc.m_Camera.m_Direction", (float *)&desc.m_Camera.m_Direction);
        serialize_float3(file, "desc.m_Camera.m_Right", (float *)&desc.m_Camera.m_Right);
        serialize_float3(file, "desc.m_Camera.m_Up", (float *)&desc.m_Camera.m_Up);

        serialize_float(file, "desc.m_Camera.m_Aspect", (float *)&desc.m_Camera.m_Aspect);
        serialize_float(file, "desc.m_Camera.m_FarPlane", (float *)&desc.m_Camera.m_FarPlane);
        serialize_float(file, "desc.m_Camera.m_NearPlane", (float *)&desc.m_Camera.m_NearPlane);
        serialize_float(file, "desc.m_Camera.m_Fov", (float *)&desc.m_Camera.m_Fov);

        serialize_float4(file, "desc.m_Camera.m_Color", (float *)&desc.m_Camera.m_Color);

        serialize_uint2(file, "desc.m_InputSize", (uint*)&desc.m_InputSize);

        serialize_uint(file, "desc.m_OutputChannelsFlag", (uint *)&desc.m_OutputChannelsFlag);

        fclose(file);
    }
    else
    {
        AMD_OUTPUT_DEBUG_STRING("AMD_AO DebugSerialize Error : Can't save AO Parameters\n");
    }

    return AOFX_RETURN_CODE_SUCCESS;
}

AMD_AOFX_DLL_API AOFX_RETURN_CODE AOFX_DebugDeserialize(AOFX_Desc& desc,
                                                        const char*                                            params,
                                                        ID3D11Texture2D**                                      ppT2D[],
                                                        ID3D11ShaderResourceView**                             ppSRV[])
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME " \n");

    HRESULT hr = S_OK;
    std::string strParams(params);
    std::wstring wstrParams(strParams.begin(), strParams.end());

    if (ppT2D != NULL && ppSRV != NULL &&
        ppT2D[0] != NULL && ppSRV[0] != NULL)
    {
        std::wstring depth = wstrParams + L".depth.dds";
        hr = DirectX::CreateDDSTextureFromFile(desc.m_pDevice, depth.c_str(), (ID3D11Resource**)ppT2D[0], ppSRV[0]);
        desc.m_pDepthSRV = *ppSRV[0];
    }
    if (ppT2D != NULL && ppSRV != NULL &&
        ppT2D[1] != NULL && ppSRV[1] != NULL)
    {
        std::wstring normal = wstrParams + L".normal.dds";
        hr = DirectX::CreateDDSTextureFromFile(desc.m_pDevice, normal.c_str(), (ID3D11Resource**)ppT2D[1], ppSRV[1]);
        desc.m_pNormalSRV = *ppSRV[1];
    }

    strParams += ".txt";
    FILE * file = fopen(strParams.c_str(), "rt");
    if (file != NULL)
    {
        char readStr[1024];
        uint multiResLayerCount = 0;

        deserialize_uint(file, readStr, (uint *)&multiResLayerCount);
        assert(multiResLayerCount == desc.m_MultiResLayerCount);

        for (uint i = 0; i < desc.m_MultiResLayerCount; i++)
        {
            deserialize_uint(file, readStr, (uint *)&desc.m_LayerProcess[i]);
            deserialize_uint(file, readStr, (uint *)&desc.m_BilateralBlurRadius[i]);
            deserialize_uint(file, readStr, (uint *)&desc.m_SampleCount[i]);
            deserialize_uint(file, readStr, (uint *)&desc.m_NormalOption[i]);
            deserialize_uint(file, readStr, (uint *)&desc.m_TapType[i]);
            deserialize_float(file, readStr, (float *)&desc.m_MultiResLayerScale[i]);
            deserialize_float(file, readStr, (float *)&desc.m_PowIntensity[i]);
            deserialize_float(file, readStr, (float *)&desc.m_RejectRadius[i]);
            deserialize_float(file, readStr, (float *)&desc.m_AcceptRadius[i]);
            deserialize_float(file, readStr, (float *)&desc.m_RecipFadeOutDist[i]);
            deserialize_float(file, readStr, (float *)&desc.m_LinearIntensity[i]);
            deserialize_float(file, readStr, (float *)&desc.m_NormalScale[i]);
            deserialize_float(file, readStr, (float *)&desc.m_ViewDistanceDiscard[i]);
            deserialize_float(file, readStr, (float *)&desc.m_ViewDistanceFade[i]);
            deserialize_float(file, readStr, (float *)&desc.m_DepthUpsampleThreshold[i]);
        }

        deserialize_uint(file, readStr, (uint *)&desc.m_Implementation);

        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_View.r[0]);
        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_View.r[1]);
        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_View.r[2]);
        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_View.r[3]);

        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_Projection.r[0]);
        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_Projection.r[1]);
        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_Projection.r[2]);
        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_Projection.r[3]);

        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_ViewProjection.r[0]);
        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_ViewProjection.r[1]);
        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_ViewProjection.r[2]);
        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_ViewProjection.r[3]);

        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_View_Inv.r[0]);
        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_View_Inv.r[1]);
        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_View_Inv.r[2]);
        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_View_Inv.r[3]);

        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_Projection_Inv.r[0]);
        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_Projection_Inv.r[1]);
        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_Projection_Inv.r[2]);
        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_Projection_Inv.r[3]);

        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_ViewProjection_Inv.r[0]);
        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_ViewProjection_Inv.r[1]);
        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_ViewProjection_Inv.r[2]);
        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_ViewProjection_Inv.r[3]);

        deserialize_float3(file, readStr, (float *)&desc.m_Camera.m_Position);
        deserialize_float3(file, readStr, (float *)&desc.m_Camera.m_Direction);
        deserialize_float3(file, readStr, (float *)&desc.m_Camera.m_Right);
        deserialize_float3(file, readStr, (float *)&desc.m_Camera.m_Up);

        deserialize_float(file, readStr, (float *)&desc.m_Camera.m_Aspect);
        deserialize_float(file, readStr, (float *)&desc.m_Camera.m_FarPlane);
        deserialize_float(file, readStr, (float *)&desc.m_Camera.m_NearPlane);
        deserialize_float(file, readStr, (float *)&desc.m_Camera.m_Fov);

        deserialize_float4(file, readStr, (float *)&desc.m_Camera.m_Color);

        deserialize_uint2(file, readStr, (uint*)&desc.m_InputSize);

        deserialize_uint(file, readStr, (uint *)&desc.m_OutputChannelsFlag);

        fclose(file);
    }

    return AOFX_RETURN_CODE_SUCCESS;
}

}

#endif