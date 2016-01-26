//
// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <d3d11_1.h>
#include <assert.h>
#include <fstream>
#include <string>

#define _USE_MATH_DEFINES
#include <cmath>

#include "AMD_LIB.h"

#if AMD_AOFX_COMPILE_DYNAMIC_LIB
# define AMD_DLL_EXPORTS
#endif

#include "AMD_AOFX_OPAQUE.h"
#include "AMD_AOFX_Precompiled.h"

#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds

// trying to optimize the blur shader further
// currently older blur version is faster by ~0.45 ms at 1080p at blur radius=16
#define USE_NEW_BLUR_PROTOTYPE 0

namespace AMD
{
const sint AOFX_OpaqueDesc::m_DeinterleaveSize[AOFX_LAYER_PROCESS_COUNT] = { 1, 2, 4, 8 };

//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
AOFX_OpaqueDesc::AOFX_OpaqueDesc(const AOFX_Desc & desc)
    : m_tbSamplePatterns(NULL)
    , m_tbSamplePatternsSRV(NULL)
    , m_psOutput(NULL)
    , m_psUpscale(NULL)
    , m_vsFullscreen(NULL)
    , m_cbSamplePatterns(NULL)
    , m_cbBilateralDilate(NULL)
    , m_cbDilateData(NULL)
    , m_ssPointClamp(NULL)
    , m_ssLinearClamp(NULL)
    , m_FormatDepth(DXGI_FORMAT_R16_FLOAT)          // DXGI_FORMAT_R32_FLOAT or DXGI_FORMAT_R16_UNORM
    , m_FormatAO(DXGI_FORMAT_R8_UNORM)           // DXGI_FORMAT_R16_FLOAT or DXGI_FORMAT_R8_UNORM
    , m_FormatDepthNormal(DXGI_FORMAT_R16G16B16A16_FLOAT) // DXGI_FORMAT_R16G16B16A16_FLOAT
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    m_Resolution.x = m_Resolution.y = 0;
    for (int i = 0; i < m_MultiResLayerCount; i++)
    {
        m_ScaledResolution[i].x = 1;
        m_ScaledResolution[i].y = 1;

        m_NormalOption[i] = AOFX_NORMAL_OPTION_NONE;
        m_LayerProcess[i] = AOFX_LAYER_PROCESS_NONE;
    }

    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
            for (int k = 0; k < 2; k++)
                m_psDilate[i][j][k] = NULL;

    for (int layer = 0; layer < AOFX_LAYER_PROCESS_COUNT; layer++)
    {
        for (int normal = 0; normal < AOFX_NORMAL_OPTION_COUNT; normal++)
        {
            for (int msaa = 0; msaa < AOFX_MSAA_LEVEL_COUNT; msaa++)
            {
                m_csProcessInput[layer][normal][msaa] = NULL;
                m_psProcessInput[layer][normal][msaa] = NULL;
            }
            for (int tap = 0; tap < AOFX_TAP_TYPE_COUNT; tap++)
            {
                for (int sample = 0; sample < AOFX_SAMPLE_COUNT_COUNT; sample++)
                {
                    m_psAmbientOcclusion[layer][normal][tap][sample] = NULL;
                    m_csAmbientOcclusion[layer][normal][tap][sample] = NULL;
                }
            }
        }
    }

    for (int radius = 0; radius < AOFX_BILATERAL_BLUR_RADIUS_COUNT; radius++)
    {
        m_csBilateralBlurUpsampling[radius] = NULL;
        m_csBilateralBlurHorizontal[radius] = NULL;
        m_csBilateralBlurVertical[radius] = NULL;
        m_csBilateralBlurH[radius] = NULL;
        m_csBilateralBlurV[radius] = NULL;
    }

    for (int i = 0; i < m_MultiResLayerCount; i++)
    {
        m_cbAOData[i] = NULL;
        m_cbAOInputData[i] = NULL;
        m_cbBilateralBlurData[i] = NULL;
    }
}

//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
AOFX_OpaqueDesc::~AOFX_OpaqueDesc()
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    release();
}

//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
AOFX_RETURN_CODE     AOFX_OpaqueDesc::cbInitialize(const AOFX_Desc & desc)
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    AOFX_RETURN_CODE              result = AOFX_RETURN_CODE_SUCCESS;
    CD3D11_DEFAULT                d3d11Default;
    CD3D11_BUFFER_DESC            b1dDesc;
    D3D11_SUBRESOURCE_DATA        subresourceData;
    CB_SAMPLEPATTERN_ROT_SINT4    cbSamplePattern;
    CB_SAMPLEPATTERN_ROT_SBYTE2   t1dSamplePattern;

    sint4  sample_i32[32];
    sbyte2 sample_i8[32];

    float fnoise = noise(0xdeadbeaf);

    for (int j = 0; j < CB_SAMPLEPATTERN_ROT_SINT4::numRotations; j++)
    {
        float seed = noise(*(uint*)&fnoise);
        srand(*(unsigned int *)&seed);
        fnoise = seed;

        memset(sample_i32, -1, sizeof(sample_i32));
        memset(sample_i8, -1, sizeof(sample_i8));

        int address_map[] =
        {
          24, 16, 25, 26,
          8, 9, 17, 27,
          0, 1, 10, 18,
          2, 3, 11, 19,
          4, 5, 12, 20,
          6, 7, 13, 21,
          14, 15, 22, 28,
          29, 23, 30, 31,
        };

        for (int i = 0; i < CB_SAMPLEPATTERN_ROT_SINT4::numSamplePatterns; i++)
        {
            int base_x = i % 4;
            int base_y = i / 4 - 4;

            float x = ((float)(rand() & 0xFFFF)) / 0xFFFF * 4.0f;
            float y = (((float)(rand() & 0xFFFF)) / 0xFFFF) * 4.0f;

            sample_i32[address_map[i]].x = (int)(base_x * 4 + x);
            sample_i32[address_map[i]].y = (int)(base_y * 4 + y);

            sample_i8[address_map[i]].x = (signed char)(base_x * 4 + x);
            sample_i8[address_map[i]].y = (signed char)(base_y * 4 + y);
        }

        memcpy(cbSamplePattern.SP[j], sample_i32, sizeof(sample_i32));
        memcpy(t1dSamplePattern.SP[j], sample_i8, sizeof(sample_i8));
    }

    memset(&subresourceData, 0, sizeof(subresourceData));

    // Sample Patterns Constant Buffer
    subresourceData.pSysMem = &cbSamplePattern;

    b1dDesc.Usage = D3D11_USAGE_DYNAMIC;
    b1dDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    b1dDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    b1dDesc.MiscFlags = 0;
    b1dDesc.ByteWidth = sizeof(CB_SAMPLEPATTERN_ROT_SINT4);
    result = (desc.m_pDevice->CreateBuffer(&b1dDesc, &subresourceData, &m_cbSamplePatterns) == S_OK ? AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_D3D11_CALL_FAILED);
    if (result != AOFX_RETURN_CODE_SUCCESS) return result;

    // Sample Patterns (TextureBuffer)
    subresourceData.pSysMem = &t1dSamplePattern;

    b1dDesc.Usage = D3D11_USAGE_DYNAMIC;
    b1dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    b1dDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    b1dDesc.MiscFlags = 0;
    b1dDesc.StructureByteStride = 0;
    b1dDesc.ByteWidth = sizeof(CB_SAMPLEPATTERN_ROT_SBYTE2);
    result = (desc.m_pDevice->CreateBuffer(&b1dDesc, &subresourceData, &m_tbSamplePatterns) == S_OK ? AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_D3D11_CALL_FAILED);
    if (result != AOFX_RETURN_CODE_SUCCESS) return result;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = CB_SAMPLEPATTERN_ROT_SBYTE2::format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = sizeof(CB_SAMPLEPATTERN_ROT_SBYTE2) / (sizeof(sbyte2));
    result = (desc.m_pDevice->CreateShaderResourceView(m_tbSamplePatterns, &srvDesc, &m_tbSamplePatternsSRV) == S_OK ? AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_D3D11_CALL_FAILED);
    if (result != AOFX_RETURN_CODE_SUCCESS) return result;

    // AO
    b1dDesc.Usage = D3D11_USAGE_DYNAMIC;
    b1dDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    b1dDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    b1dDesc.MiscFlags = 0;
    b1dDesc.ByteWidth = sizeof(AO_Data);
    for (int i = 0; i < m_MultiResLayerCount; i++)
    {
        result = (desc.m_pDevice->CreateBuffer(&b1dDesc, NULL, &m_cbAOData[i]) == S_OK ? AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_D3D11_CALL_FAILED);
        if (result != AOFX_RETURN_CODE_SUCCESS) return result;
    }

    // Bilateral Dilate
    b1dDesc.Usage = D3D11_USAGE_DYNAMIC;
    b1dDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    b1dDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    b1dDesc.MiscFlags = 0;
    b1dDesc.ByteWidth = sizeof(CB_BILATERAL_DILATE);
    result = (desc.m_pDevice->CreateBuffer(&b1dDesc, NULL, &m_cbBilateralDilate) == S_OK ? AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_D3D11_CALL_FAILED);
    if (result != AOFX_RETURN_CODE_SUCCESS) return result;

    // Dilate
    b1dDesc.Usage = D3D11_USAGE_DYNAMIC;
    b1dDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    b1dDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    b1dDesc.MiscFlags = 0;
    b1dDesc.ByteWidth = sizeof(S_DILATE_DATA);
    result = (desc.m_pDevice->CreateBuffer(&b1dDesc, NULL, &m_cbDilateData) == S_OK ? AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_D3D11_CALL_FAILED);
    if (result != AOFX_RETURN_CODE_SUCCESS) return result;

    // Deinterleave & Scale pre-processing parameters
    b1dDesc.Usage = D3D11_USAGE_DYNAMIC;
    b1dDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    b1dDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    b1dDesc.MiscFlags = 0;
    b1dDesc.ByteWidth = sizeof(AO_InputData);
    for (int i = 0; i < m_MultiResLayerCount; i++)
    {
        result = (desc.m_pDevice->CreateBuffer(&b1dDesc, NULL, &m_cbAOInputData[i]) == S_OK ? AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_D3D11_CALL_FAILED);
        if (result != AOFX_RETURN_CODE_SUCCESS) return result;
        result = (desc.m_pDevice->CreateBuffer(&b1dDesc, NULL, &m_cbBilateralBlurData[i]) == S_OK ? AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_D3D11_CALL_FAILED);
        if (result != AOFX_RETURN_CODE_SUCCESS) return result;
    }

    // Point Clamp
    CD3D11_SAMPLER_DESC samplerDesc(d3d11Default);
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    memset(samplerDesc.BorderColor, 0, sizeof(samplerDesc.BorderColor));
    result = (desc.m_pDevice->CreateSamplerState(&samplerDesc, &m_ssPointClamp) == S_OK ? AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_D3D11_CALL_FAILED);
    if (result != AOFX_RETURN_CODE_SUCCESS) return result;

    // Linear Clamp
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    result = (desc.m_pDevice->CreateSamplerState(&samplerDesc, &m_ssLinearClamp) == S_OK ? AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_D3D11_CALL_FAILED);
    if (result != AOFX_RETURN_CODE_SUCCESS) return result;

    CD3D11_BLEND_DESC blendDesc(d3d11Default);
    m_bsOutputChannel[0] = NULL;
    for (int blend = AOFX_OUTPUT_CHANNEL_R; blend < AOFX_OUTPUT_CHANNEL_COUNT; blend++)
    {
        blendDesc.RenderTarget[0].RenderTargetWriteMask = (UINT8)blend;
        result = (desc.m_pDevice->CreateBlendState(&blendDesc, &m_bsOutputChannel[blend]) == S_OK ? AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_D3D11_CALL_FAILED);
        if (result != AOFX_RETURN_CODE_SUCCESS) return result;
    }

    CD3D11_RASTERIZER_DESC rsDesc(d3d11Default);
    rsDesc.CullMode = D3D11_CULL_NONE;
    result = (desc.m_pDevice->CreateRasterizerState(&rsDesc, &m_rsNoCulling) == S_OK ? AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_D3D11_CALL_FAILED);

    return result;
}

//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
AOFX_RETURN_CODE  AOFX_OpaqueDesc::resize(const AOFX_Desc & desc)
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    AOFX_RETURN_CODE result = AOFX_RETURN_CODE_SUCCESS;

    uint width = (uint)desc.m_InputSize.x;
    uint height = (uint)desc.m_InputSize.y;

    if (width == 0 || height == 0)
    {
        return AOFX_RETURN_CODE_INVALID_ARGUMENT;
    }

    bool resolutionChanged = (m_Resolution.x != width || m_Resolution.y != height);

    if (resolutionChanged)
    {
        m_DilateAO.Release();

        result = m_DilateAO.CreateSurface(desc.m_pDevice, width, height, 1, 1, 1,
                                          m_FormatAO, m_FormatAO, m_FormatAO, DXGI_FORMAT_UNKNOWN, m_FormatAO, DXGI_FORMAT_UNKNOWN, D3D11_USAGE_DEFAULT, false, 0, NULL, NULL, 0) == S_OK ?
            AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_D3D11_CALL_FAILED;
        if (result != AOFX_RETURN_CODE_SUCCESS) return result;

        m_Resolution.x = width;
        m_Resolution.y = height;
    }

    for (int i = 0; i < m_MultiResLayerCount; ++i)
    {
        if (desc.m_LayerProcess[i] == AOFX_LAYER_PROCESS_NONE) // if the layer is currently disabled
        {
            m_AO[i].Release();
            m_ResultAO[i].Release();
            m_InputAO[i].Release();

            memset(&m_aoData[i], 0, sizeof(m_aoData[i]));
            memset(&m_aoInputData[i], 0, sizeof(m_aoInputData[i]));
            memset(&m_aoBilateralBlurData[i], 0, sizeof(m_aoBilateralBlurData[i]));
        }
        else
        {
            if (m_LayerProcess[i] == AOFX_LAYER_PROCESS_NONE || // if the layer was just enabled
                resolutionChanged)
            {
                m_AO[i].Release();
                result = m_AO[i].CreateSurface(desc.m_pDevice, width, height, 1, 1, 1,
                                               m_FormatAO, m_FormatAO, m_FormatAO, DXGI_FORMAT_UNKNOWN, m_FormatAO, DXGI_FORMAT_UNKNOWN, D3D11_USAGE_DEFAULT, false, 0, NULL, NULL, 0) == S_OK ?
                    AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_D3D11_CALL_FAILED;
                if (result != AOFX_RETURN_CODE_SUCCESS) return result;
            }

            DXGI_FORMAT format;
            switch (desc.m_NormalOption[i])
            {
            case AOFX_NORMAL_OPTION_NONE: format = m_FormatDepth; break;
            case AOFX_NORMAL_OPTION_READ_FROM_SRV: format = m_FormatDepthNormal; break;
            default: format = DXGI_FORMAT_UNKNOWN; break;
            }

            bool modeChanged = (desc.m_LayerProcess[i] != m_LayerProcess[i]);
            bool normalChanged = (desc.m_NormalOption[i] != m_NormalOption[i]);

            uint scaledWidth = MAX((uint)(desc.m_InputSize.x * desc.m_MultiResLayerScale[i]), (uint)1);
            uint scaledHeight = MAX((uint)(desc.m_InputSize.y * desc.m_MultiResLayerScale[i]), (uint)1);
            bool scaleChanged = (m_ScaledResolution[i].x != scaledWidth && m_ScaledResolution[i].y != scaledHeight);

            bool resizeResources = modeChanged || resolutionChanged || normalChanged || scaleChanged;

            if (resizeResources)
            {
                int dienterleaveSize = m_DeinterleaveSize[desc.m_LayerProcess[i]];

                uint deinterleavedWidth = (uint)ceilf((float)scaledWidth / dienterleaveSize);
                uint deinterleavedHeight = (uint)ceilf((float)scaledHeight / dienterleaveSize);
                uint deinterleavedArray = dienterleaveSize * dienterleaveSize;

                m_ResultAO[i].Release();

                result = m_ResultAO[i].CreateSurface(desc.m_pDevice,
                                                     scaledWidth, scaledHeight, 1, 1, 1,
                                                     m_FormatAO, m_FormatAO, m_FormatAO,
                                                     DXGI_FORMAT_UNKNOWN, m_FormatAO, DXGI_FORMAT_UNKNOWN, D3D11_USAGE_DEFAULT, false, 0, NULL, NULL, 0) == S_OK ?
                    AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_D3D11_CALL_FAILED;
                if (result != AOFX_RETURN_CODE_SUCCESS) return result;

                m_InputAO[i].Release();
                result = m_InputAO[i].CreateSurface(desc.m_pDevice,
                                                    deinterleavedWidth, deinterleavedHeight, 1, deinterleavedArray, 1,
                                                    format, format, format, DXGI_FORMAT_UNKNOWN, format, DXGI_FORMAT_UNKNOWN, D3D11_USAGE_DEFAULT, false, 0, NULL, NULL, 0) == S_OK ?
                    AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_D3D11_CALL_FAILED;
                if (result != AOFX_RETURN_CODE_SUCCESS) return result;

                m_ScaledResolution[i].x = scaledWidth;
                m_ScaledResolution[i].y = scaledHeight;
            }
        }

        m_LayerProcess[i] = desc.m_LayerProcess[i];
        m_NormalOption[i] = desc.m_NormalOption[i];
    }

    // Setup the constant buffer for the AO shaders
    D3D11_MAPPED_SUBRESOURCE mappedResource;

    // Setup the constant buffer for the BD shaders
    desc.m_pDeviceContext->Map(m_cbBilateralDilate, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    CB_BILATERAL_DILATE* pBilateralDilate = (CB_BILATERAL_DILATE*)mappedResource.pData;
    pBilateralDilate->m_OutputSize.x = (float)desc.m_InputSize.x;
    pBilateralDilate->m_OutputSize.y = (float)desc.m_InputSize.y;
    pBilateralDilate->m_OutputSize.z = 1.0f / desc.m_InputSize.x;
    pBilateralDilate->m_OutputSize.w = 1.0f / desc.m_InputSize.y;
    pBilateralDilate->m_CameraQ = desc.m_Camera.m_FarPlane / (desc.m_Camera.m_FarPlane - desc.m_Camera.m_NearPlane); //camera_far_clip / ( camera_far_clip - camera_near_clip );
    pBilateralDilate->m_CameraQTimesZNear = pBilateralDilate->m_CameraQ * desc.m_Camera.m_NearPlane; //cameraQ * camera_near_clip;
    desc.m_pDeviceContext->Unmap(m_cbBilateralDilate, 0);

    return result;
}

//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
AOFX_RETURN_CODE AOFX_OpaqueDesc::createShaders(const AOFX_Desc & desc)
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    releaseShaders();

    AOFX_RETURN_CODE result = AOFX_RETURN_CODE_SUCCESS;
    HRESULT hr = S_OK;

    hr = desc.m_pDevice->CreatePixelShader(PS_OUTPUT_Data, sizeof(PS_OUTPUT_Data), NULL, &m_psOutput);
    if (hr != S_OK) return AOFX_RETURN_CODE_D3D11_CALL_FAILED;
    hr = desc.m_pDevice->CreatePixelShader(PS_UPSAMPLE_Data, sizeof(PS_UPSAMPLE_Data), NULL, &m_psUpscale);
    if (hr != S_OK) return AOFX_RETURN_CODE_D3D11_CALL_FAILED;

    for (int radius = 0; radius < AOFX_BILATERAL_BLUR_RADIUS_COUNT; radius++)
    {
        hr = desc.m_pDevice->CreateComputeShader(CS_AMD_BLUR_Data[radius * 2 + 0], CS_AMD_BLUR_Size[radius * 2 + 0], NULL, &m_csBilateralBlurH[radius]);
        if (result != S_OK) return AOFX_RETURN_CODE_D3D11_CALL_FAILED;
        hr = desc.m_pDevice->CreateComputeShader(CS_AMD_BLUR_Data[radius * 2 + 1], CS_AMD_BLUR_Size[radius * 2 + 1], NULL, &m_csBilateralBlurV[radius]);
        if (result != S_OK) return AOFX_RETURN_CODE_D3D11_CALL_FAILED;
        hr = desc.m_pDevice->CreateComputeShader(CS_BILATERAL_BLUR_Data[radius], CS_BILATERAL_BLUR_Size[radius], NULL, &m_csBilateralBlurUpsampling[radius]);
        if (result != S_OK) return AOFX_RETURN_CODE_D3D11_CALL_FAILED;

        hr = desc.m_pDevice->CreateComputeShader(CS_BILATERAL_BLUR_SEPARABLE_Data[radius * 2 + 0], CS_BILATERAL_BLUR_SEPARABLE_Size[radius * 2 + 0], NULL, &m_csBilateralBlurHorizontal[radius]);
        if (result != S_OK) return AOFX_RETURN_CODE_D3D11_CALL_FAILED;
        hr = desc.m_pDevice->CreateComputeShader(CS_BILATERAL_BLUR_SEPARABLE_Data[radius * 2 + 1], CS_BILATERAL_BLUR_SEPARABLE_Size[radius * 2 + 1], NULL, &m_csBilateralBlurVertical[radius]);
        if (result != S_OK) return AOFX_RETURN_CODE_D3D11_CALL_FAILED;
    }

    hr = desc.m_pDevice->CreateVertexShader(VS_FULLSCREEN_Data, sizeof(VS_FULLSCREEN_Data), NULL, &m_vsFullscreen);
    if (hr != S_OK) return AOFX_RETURN_CODE_D3D11_CALL_FAILED;

    int counter = 0;
    for (int layer = 0; layer < AOFX_LAYER_PROCESS_COUNT; layer++)
    {
        for (int normal = 0; normal < AOFX_NORMAL_OPTION_COUNT; normal++)
        {
            for (int tap = 0; tap < AOFX_TAP_TYPE_COUNT; tap++)
            {
                for (int sample = 0; sample < AOFX_SAMPLE_COUNT_COUNT; sample++)
                {
                    hr = desc.m_pDevice->CreatePixelShader(PS_AMD_AO_Data[counter], PS_AMD_AO_Size[counter], NULL, &m_psAmbientOcclusion[layer][normal][tap][sample]);
                    if (hr != S_OK) return AOFX_RETURN_CODE_D3D11_CALL_FAILED;
                    hr = desc.m_pDevice->CreateComputeShader(CS_AMD_AO_Data[counter], CS_AMD_AO_Size[counter], NULL, &m_csAmbientOcclusion[layer][normal][tap][sample]);
                    if (hr != S_OK) return AOFX_RETURN_CODE_D3D11_CALL_FAILED;
                    counter++;
                }
            }
        }
    }

    counter = 0;
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                if (!(i == 0 && j == 0 && k == 0))
                {
                    hr = desc.m_pDevice->CreatePixelShader(PS_AO_DILATE_Data[counter], PS_AO_DILATE_Size[counter], NULL, &m_psDilate[k][j][i]);
                    if (hr != S_OK) return AOFX_RETURN_CODE_D3D11_CALL_FAILED;
                }
                counter++;
            }
        }
    }

    counter = 0;
    for (int layer = 0; layer < AOFX_LAYER_PROCESS_COUNT; layer++)
    {
        for (int normal = 0; normal < AOFX_NORMAL_OPTION_COUNT; normal++)
        {
            for (int msaa = 0; msaa < AOFX_MSAA_LEVEL_COUNT; msaa++)
            {
                hr = desc.m_pDevice->CreateComputeShader(CS_AO_DEINTERLEAVE_Data[counter], CS_AO_DEINTERLEAVE_Size[counter], NULL, &m_csProcessInput[layer][normal][msaa]);
                if (hr != S_OK) return AOFX_RETURN_CODE_D3D11_CALL_FAILED;
                hr = desc.m_pDevice->CreatePixelShader(PS_AO_DEINTERLEAVE_Data[counter], PS_AO_DEINTERLEAVE_Size[counter], NULL, &m_psProcessInput[layer][normal][msaa]);
                if (hr != S_OK) return AOFX_RETURN_CODE_D3D11_CALL_FAILED;
                counter++;
            }
        }
    }

    return AOFX_RETURN_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
void AOFX_OpaqueDesc::releaseShaders()
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    AMD_SAFE_RELEASE(m_psOutput);
    AMD_SAFE_RELEASE(m_psUpscale);

    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
            for (int k = 0; k < 2; k++)
                AMD_SAFE_RELEASE(m_psDilate[i][j][k]);

    for (int radius = 0; radius < AOFX_BILATERAL_BLUR_RADIUS_COUNT; radius++)
    {
        AMD_SAFE_RELEASE(m_csBilateralBlurUpsampling[radius]);
        AMD_SAFE_RELEASE(m_csBilateralBlurH[radius]);
        AMD_SAFE_RELEASE(m_csBilateralBlurV[radius]);
        AMD_SAFE_RELEASE(m_csBilateralBlurHorizontal[radius]);
        AMD_SAFE_RELEASE(m_csBilateralBlurVertical[radius]);
    }

    AMD_SAFE_RELEASE(m_vsFullscreen);

    for (int layer = 0; layer < AOFX_LAYER_PROCESS_COUNT; layer++)
    {
        for (int normal = 0; normal < AOFX_NORMAL_OPTION_COUNT; normal++)
        {
            for (int msaa = 0; msaa < AOFX_MSAA_LEVEL_COUNT; msaa++)
            {
                AMD_SAFE_RELEASE(m_csProcessInput[layer][normal][msaa]);
                AMD_SAFE_RELEASE(m_psProcessInput[layer][normal][msaa]);
            }
            for (int tap = 0; tap < AOFX_TAP_TYPE_COUNT; tap++)
            {
                for (int sample = 0; sample < AOFX_SAMPLE_COUNT_COUNT; sample++)
                {
                    AMD_SAFE_RELEASE(m_psAmbientOcclusion[layer][normal][tap][sample]);
                    AMD_SAFE_RELEASE(m_csAmbientOcclusion[layer][normal][tap][sample]);
                }
            }
        }
    }
}

//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
void AOFX_OpaqueDesc::releaseTextures()
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    m_Resolution.x = m_Resolution.y = 0;

    m_DilateAO.Release();

    for (int i = 0; i < AOFX_OpaqueDesc::m_MultiResLayerCount; ++i)
    {
        memset(&m_aoData[i], 0, sizeof(m_aoData[i]));
        memset(&m_aoInputData[i], 0, sizeof(m_aoInputData[i]));
        memset(&m_aoBilateralBlurData[i], 0, sizeof(m_aoBilateralBlurData[i]));

        m_ScaledResolution[i].x = 0;
        m_ScaledResolution[i].y = 0;

        m_NormalOption[i] = AOFX_NORMAL_OPTION_COUNT;
        m_LayerProcess[i] = AOFX_LAYER_PROCESS_NONE;

        m_AO[i].Release();
        m_ResultAO[i].Release();
        m_InputAO[i].Release();
    }
}

//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
void AOFX_OpaqueDesc::release()
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    releaseShaders();
    releaseTextures();

    for (int blend = AOFX_OUTPUT_CHANNEL_R; blend < AOFX_OUTPUT_CHANNEL_COUNT; blend++)
    {
        AMD_SAFE_RELEASE(m_bsOutputChannel[blend]);
    }

    AMD_SAFE_RELEASE(m_rsNoCulling);

    AMD_SAFE_RELEASE(m_tbSamplePatterns);
    AMD_SAFE_RELEASE(m_tbSamplePatternsSRV);
    AMD_SAFE_RELEASE(m_cbSamplePatterns);

    for (int i = 0; i < m_MultiResLayerCount; i++)
    {
        AMD_SAFE_RELEASE(m_cbAOData[i]);
        AMD_SAFE_RELEASE(m_cbAOInputData[i]);
        AMD_SAFE_RELEASE(m_cbBilateralBlurData[i]);
    }

    AMD_SAFE_RELEASE(m_cbBilateralDilate);
    AMD_SAFE_RELEASE(m_cbDilateData);
    AMD_SAFE_RELEASE(m_ssPointClamp);
    AMD_SAFE_RELEASE(m_ssLinearClamp);
}

//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
AOFX_RETURN_CODE  AOFX_OpaqueDesc::csProcessInput(uint target, const AOFX_Desc & desc)
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    ID3D11RenderTargetView*  pNullRTV[8] = { 0 };
    desc.m_pDeviceContext->OMSetRenderTargets(AMD_ARRAY_SIZE(pNullRTV), pNullRTV, NULL);

    AO_InputData aoInputData(desc, target);

    int deinterleaveSize = m_DeinterleaveSize[desc.m_LayerProcess[target]];
    uint scaledWidth = MAX((uint)(desc.m_InputSize.x * desc.m_MultiResLayerScale[target]), (uint)1);
    uint scaledHeight = MAX((uint)(desc.m_InputSize.y * desc.m_MultiResLayerScale[target]), (uint)1);
    uint deinterleavedScaledWidth = (uint)ceilf((float)scaledWidth / deinterleaveSize);
    uint deinterleavedScaledHeight = (uint)ceilf((float)scaledHeight / deinterleaveSize);

    aoInputData.m_OutputSize.x = deinterleavedScaledWidth;
    aoInputData.m_OutputSize.y = deinterleavedScaledHeight;
    aoInputData.m_OutputSizeRcp.x = 1.0f / aoInputData.m_OutputSize.x;
    aoInputData.m_OutputSizeRcp.y = 1.0f / aoInputData.m_OutputSize.y;
    aoInputData.m_InputSize.x = scaledWidth;
    aoInputData.m_InputSize.y = scaledHeight;
    aoInputData.m_InputSizeRcp.x = 1.0f / aoInputData.m_InputSize.x;
    aoInputData.m_InputSizeRcp.y = 1.0f / aoInputData.m_InputSize.y;

    assert(sizeof(m_aoInputData[target]) == sizeof(aoInputData));
    if (0 != memcmp(&m_aoInputData[target], &aoInputData, sizeof(m_aoInputData[target])))
    {
        D3D11_MAPPED_SUBRESOURCE   mappedResource;
        desc.m_pDeviceContext->Map(m_cbAOInputData[target], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        memcpy(mappedResource.pData, &aoInputData, sizeof(aoInputData));
        desc.m_pDeviceContext->Unmap(m_cbAOInputData[target], 0);
        memcpy(&m_aoInputData[target], &aoInputData, sizeof(aoInputData));
    }

    ID3D11Buffer*              pCB[] = { m_cbAOInputData[target] };
    ID3D11UnorderedAccessView* pUAV[] = { m_InputAO[target]._uav };
    ID3D11ShaderResourceView*  pSRV[] = { desc.m_pDepthSRV, desc.m_pNormalSRV };
    ID3D11SamplerState*        pSS[] = { m_ssPointClamp, m_ssLinearClamp };

    desc.m_pDeviceContext->CSSetConstantBuffers(0, AMD_ARRAY_SIZE(pCB), pCB);
    desc.m_pDeviceContext->CSSetUnorderedAccessViews(0, AMD_ARRAY_SIZE(pUAV), pUAV, NULL);
    desc.m_pDeviceContext->CSSetShaderResources(0, AMD_ARRAY_SIZE(pSRV), pSRV);
    desc.m_pDeviceContext->CSSetSamplers(0, AMD_ARRAY_SIZE(pSS), pSS);

    uint uX = (int)ceilf((float)aoInputData.m_InputSize.x / m_DeinterleaveGroupDim);
    uint uY = (int)ceilf((float)aoInputData.m_InputSize.y / m_DeinterleaveGroupDim);

    desc.m_pDeviceContext->CSSetShader(m_csProcessInput[desc.m_LayerProcess[target]][desc.m_NormalOption[target]][AOFX_MSAA_LEVEL_1], NULL, 0);

    desc.m_pDeviceContext->Dispatch(uX, uY, 1);

    ID3D11ShaderResourceView*  pNullSRV[AMD_ARRAY_SIZE(pSRV)] = { 0 };
    ID3D11UnorderedAccessView* pNullUAV[AMD_ARRAY_SIZE(pUAV)] = { 0 };
    desc.m_pDeviceContext->CSSetUnorderedAccessViews(0, AMD_ARRAY_SIZE(pNullUAV), pNullUAV, NULL);
    desc.m_pDeviceContext->CSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);

    return AOFX_RETURN_CODE_SUCCESS;
}

AOFX_RETURN_CODE  AOFX_OpaqueDesc::psProcessInput(uint target, const AOFX_Desc & desc)
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    AO_InputData aoInputData(desc, target);

    int deinterleaveSize = m_DeinterleaveSize[desc.m_LayerProcess[target]];
    uint scaledWidth = MAX((uint)(desc.m_InputSize.x * desc.m_MultiResLayerScale[target]), (uint)1);
    uint scaledHeight = MAX((uint)(desc.m_InputSize.y * desc.m_MultiResLayerScale[target]), (uint)1);
    uint deinterleavedScaledWidth = (uint)ceilf((float)scaledWidth / deinterleaveSize);
    uint deinterleavedScaledHeight = (uint)ceilf((float)scaledHeight / deinterleaveSize);

    aoInputData.m_OutputSize.x = deinterleavedScaledWidth;
    aoInputData.m_OutputSize.y = deinterleavedScaledHeight;
    aoInputData.m_OutputSizeRcp.x = 1.0f / aoInputData.m_OutputSize.x;
    aoInputData.m_OutputSizeRcp.y = 1.0f / aoInputData.m_OutputSize.y;
    aoInputData.m_InputSize.x = desc.m_InputSize.x;
    aoInputData.m_InputSize.y = desc.m_InputSize.y;
    aoInputData.m_InputSizeRcp.x = 1.0f / desc.m_InputSize.x;
    aoInputData.m_InputSizeRcp.y = 1.0f / desc.m_InputSize.y;

    assert(sizeof(m_aoInputData[target]) == sizeof(aoInputData));
    if (0 != memcmp(&m_aoInputData[target], &aoInputData, sizeof(m_aoInputData[target])))
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        desc.m_pDeviceContext->Map(m_cbAOInputData[target], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        memcpy(mappedResource.pData, &aoInputData, sizeof(aoInputData));
        desc.m_pDeviceContext->Unmap(m_cbAOInputData[target], 0);
        memcpy(&m_aoInputData[target], &aoInputData, sizeof(aoInputData));
    }

    CD3D11_VIEWPORT vpDeinterleaved(0.0f, 0.0f, (float)deinterleavedScaledWidth * deinterleaveSize, (float)deinterleavedScaledHeight * deinterleaveSize, 0.0f, 1.0f);

    ID3D11Buffer*             pCB[] = { m_cbAOInputData[target] };
    ID3D11ShaderResourceView* pSRV[] = { desc.m_pDepthSRV, desc.m_pNormalSRV };
    ID3D11SamplerState*       pSS[] = { m_ssPointClamp, m_ssLinearClamp };

    HRESULT hr = AMD::RenderFullscreenPass(desc.m_pDeviceContext,
                                           vpDeinterleaved,
                                           m_vsFullscreen,
                                           m_psProcessInput[desc.m_LayerProcess[target]][desc.m_NormalOption[target]][AOFX_MSAA_LEVEL_1],
                                           NULL, 0,
                                           pCB, AMD_ARRAY_SIZE(pCB),
                                           pSS, AMD_ARRAY_SIZE(pSS),
                                           pSRV, AMD_ARRAY_SIZE(pSRV),
                                           NULL, 0,
                                           &m_InputAO[target]._uav, 0, 1,
                                           NULL, NULL, 0, m_bsOutputChannel[0xf],
                                           m_rsNoCulling);

    return hr == S_OK ? AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_FAIL;
}


//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
AOFX_RETURN_CODE  AOFX_OpaqueDesc::csAmbientOcclusion(uint target, const AOFX_Desc & desc)
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    AO_Data                    aoData(desc, target);

    ID3D11Buffer*              pCB[] = { m_cbAOData[target], m_cbSamplePatterns };
    ID3D11SamplerState*        pSS[] = { m_ssPointClamp, m_ssLinearClamp };
    ID3D11ShaderResourceView*  pSRV[] = { desc.m_pDepthSRV, desc.m_pNormalSRV, m_tbSamplePatternsSRV, m_InputAO[target]._srv };
    ID3D11UnorderedAccessView* pUAV[] = { m_AO[target]._uav };

    if (desc.m_MultiResLayerScale[target] < 1.0f)
        pUAV[0] = m_ResultAO[target]._uav;

    int deinterleaveSize = m_DeinterleaveSize[desc.m_LayerProcess[target]];
    uint scaledWidth = MAX((uint)(desc.m_InputSize.x * desc.m_MultiResLayerScale[target]), (uint)1);
    uint scaledHeight = MAX((uint)(desc.m_InputSize.y * desc.m_MultiResLayerScale[target]), (uint)1);
    uint deinterleavedScaledWidth = (uint)ceilf((float)scaledWidth / deinterleaveSize);
    uint deinterleavedScaledHeight = (uint)ceilf((float)scaledHeight / deinterleaveSize);

    aoData.m_InputSize.x = scaledWidth;
    aoData.m_InputSize.y = scaledHeight;
    aoData.m_InputSizeRcp.x = 2.0f / aoData.m_InputSize.x; // we are actually passing 2.0 * 1 / m_InputSize 
    aoData.m_InputSizeRcp.y = 2.0f / aoData.m_InputSize.y; // because the shader always does 2.0 * 1 / InputSize
    aoData.m_OutputSize.x = deinterleavedScaledWidth;
    aoData.m_OutputSize.y = deinterleavedScaledHeight;
    aoData.m_OutputSizeRcp.x = (float) 1.0f / aoData.m_OutputSize.x;  // this value needs to be adjusted because it
    aoData.m_OutputSizeRcp.y = (float) 1.0f / aoData.m_OutputSize.y;  // participates in clip-space - to - world space transformation

    assert(sizeof(m_aoData[target]) == sizeof(aoData));
    if (0 != memcmp(&m_aoData[target], &aoData, sizeof(m_aoData[target])))
    {
        D3D11_MAPPED_SUBRESOURCE   mappedResource;
        desc.m_pDeviceContext->Map(m_cbAOData[target], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        memcpy(mappedResource.pData, &aoData, sizeof(aoData));
        desc.m_pDeviceContext->Unmap(m_cbAOData[target], 0);
        memcpy(&m_aoData[target], &aoData, sizeof(m_aoData[target]));
    }

    desc.m_pDeviceContext->CSSetSamplers(0, AMD_ARRAY_SIZE(pSS), pSS);
    desc.m_pDeviceContext->CSSetConstantBuffers(0, AMD_ARRAY_SIZE(pCB), pCB);
    desc.m_pDeviceContext->CSSetShaderResources(0, AMD_ARRAY_SIZE(pSRV), pSRV);
    desc.m_pDeviceContext->CSSetUnorderedAccessViews(0, AMD_ARRAY_SIZE(pUAV), pUAV, NULL);

    uint uGridX = (uint)ceilf((float)aoData.m_OutputSize.x / m_DeinterleaveGroupDim);
    uint uGridY = (uint)ceilf((float)aoData.m_OutputSize.y / m_DeinterleaveGroupDim);

    desc.m_pDeviceContext->CSSetShader(m_csAmbientOcclusion[desc.m_LayerProcess[target]][desc.m_NormalOption[target]][desc.m_TapType[target]][desc.m_SampleCount[target]], NULL, 0);

    desc.m_pDeviceContext->Dispatch(uGridX * deinterleaveSize, uGridY * deinterleaveSize, 1);

    ID3D11ShaderResourceView*  pNullSRV[8] = { 0 };
    ID3D11UnorderedAccessView* pNullUAV[8] = { 0 };
    desc.m_pDeviceContext->CSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);
    desc.m_pDeviceContext->CSSetUnorderedAccessViews(0, AMD_ARRAY_SIZE(pNullUAV), pNullUAV, NULL);

    return AOFX_RETURN_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
AOFX_RETURN_CODE  AOFX_OpaqueDesc::psAmbientOcclusion(uint target, const AOFX_Desc & desc)
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    AO_Data                   aoData(desc, target);

    int deinterleaveSize = m_DeinterleaveSize[desc.m_LayerProcess[target]];
    uint scaledWidth = MAX((uint)(desc.m_InputSize.x * desc.m_MultiResLayerScale[target]), (uint)1);
    uint scaledHeight = MAX((uint)(desc.m_InputSize.y * desc.m_MultiResLayerScale[target]), (uint)1);
    uint deinterleavedScaledWidth = (uint)ceilf((float)scaledWidth / deinterleaveSize);
    uint deinterleavedScaledHeight = (uint)ceilf((float)scaledHeight / deinterleaveSize);

    CD3D11_VIEWPORT            VP(0.0f, 0.0f,
                                  (float)deinterleavedScaledWidth * deinterleaveSize,
                                  (float)deinterleavedScaledHeight * deinterleaveSize, 0.0f, 1.0f);
    D3D11_RECT*                pNullSR = NULL;
    ID3D11SamplerState*        pSS[] = { m_ssPointClamp, m_ssLinearClamp };
    ID3D11ShaderResourceView*  pSRV[] = { desc.m_pDepthSRV, desc.m_pNormalSRV, m_tbSamplePatternsSRV, m_InputAO[target]._srv };
    ID3D11Buffer*              pCB[] = { m_cbAOData[target], m_cbSamplePatterns };
    ID3D11UnorderedAccessView* pUAV[] = { m_AO[target]._uav };

    if (desc.m_MultiResLayerScale[target] < 1.0f)
        pUAV[0] = m_ResultAO[target]._uav;

    aoData.m_OutputSize.x = deinterleavedScaledWidth;
    aoData.m_OutputSize.y = deinterleavedScaledHeight;
    aoData.m_OutputSizeRcp.x = 1.0f / aoData.m_OutputSize.x;
    aoData.m_OutputSizeRcp.y = 1.0f / aoData.m_OutputSize.y;

    aoData.m_InputSize.x = scaledWidth;
    aoData.m_InputSize.y = scaledHeight;
    aoData.m_InputSizeRcp.x = 2.0f / aoData.m_InputSize.x; // we are actually passing 2.0 * 1 / m_InputSize 
    aoData.m_InputSizeRcp.y = 2.0f / aoData.m_InputSize.y; // because the shader always does 2.0 * 1 / InputSize

    assert(sizeof(m_aoData[target]) == sizeof(aoData));
    if ( 0 != memcmp(&m_aoData[target], &aoData, sizeof(m_aoData[target])))
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        desc.m_pDeviceContext->Map(m_cbAOData[target], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        memcpy(mappedResource.pData, &aoData, sizeof(aoData));
        desc.m_pDeviceContext->Unmap(m_cbAOData[target], 0);
        memcpy(&m_aoData[target], &aoData, sizeof(m_aoData[target]));
    }

    HRESULT hr = AMD::RenderFullscreenPass(desc.m_pDeviceContext, VP,
                                           m_vsFullscreen,
                                           m_psAmbientOcclusion[desc.m_LayerProcess[target]][desc.m_NormalOption[target]][desc.m_TapType[target]][desc.m_SampleCount[target]],
                                           pNullSR, 0,
                                           pCB, AMD_ARRAY_SIZE(pCB),
                                           pSS, AMD_ARRAY_SIZE(pSS),
                                           pSRV, AMD_ARRAY_SIZE(pSRV),
                                           NULL, 0,
                                           pUAV, 0, AMD_ARRAY_SIZE(pUAV),
                                           NULL, NULL, 0,
                                           m_bsOutputChannel[15], m_rsNoCulling);

    return hr == S_OK ? AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_FAIL;
}


  //-------------------------------------------------------------------------------------------------
  // 
  //-------------------------------------------------------------------------------------------------
AOFX_RETURN_CODE  AOFX_OpaqueDesc::csBlurAO(uint target, const AOFX_Desc & desc)
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    // this part may be confusing to the observer
    // if all AO layers had the same blur radius, then csBlurAO will have a custom behaviour
    // - it expects input in Dilate.srv
    // - it will use AO[0] layer parameters such as Blur Radius (which is ok, all radiuses are equal) and aoInputData
    // - and it will use AO[0] resource as intermidiate buffer
    // otherwise it will just blur the 'target' layer
    // TODO : actually layer '0' may be disabled so using this layer is not safe
    uint selectTarget = target != m_MultiResLayerCount ? target : 0;

    AO_InputData aoInputData(desc, selectTarget);

    aoInputData.m_OutputSize.x = desc.m_InputSize.x;
    aoInputData.m_OutputSize.y = desc.m_InputSize.y;
    aoInputData.m_OutputSizeRcp.x = 1.0f / aoInputData.m_OutputSize.x;
    aoInputData.m_OutputSizeRcp.y = 1.0f / aoInputData.m_OutputSize.y;
    aoInputData.m_InputSize.x = desc.m_InputSize.x;
    aoInputData.m_InputSize.y = desc.m_InputSize.y;
    aoInputData.m_InputSizeRcp.x = 1.0f / aoInputData.m_InputSize.x;
    aoInputData.m_InputSizeRcp.y = 1.0f / aoInputData.m_InputSize.y;

    assert(sizeof(m_aoBilateralBlurData[selectTarget]) == sizeof(aoInputData));
    if (0 != memcmp(&m_aoBilateralBlurData[selectTarget], &aoInputData, sizeof(m_aoBilateralBlurData[selectTarget])))
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        desc.m_pDeviceContext->Map(m_cbBilateralBlurData[selectTarget], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        memcpy(mappedResource.pData, &aoInputData, sizeof(aoInputData));
        desc.m_pDeviceContext->Unmap(m_cbBilateralBlurData[selectTarget], 0);
        memcpy(&m_aoBilateralBlurData[selectTarget], &aoInputData, sizeof(aoInputData));
    }

    ID3D11ShaderResourceView*  pNullSRV[8] = { 0 };
    ID3D11UnorderedAccessView* pNullUAV[8] = { 0 };
    ID3D11ShaderResourceView*  pSRV[] = { desc.m_pDepthSRV, desc.m_pNormalSRV, m_AO[selectTarget]._srv };
    ID3D11Buffer*              pCB[] = { m_cbBilateralBlurData[selectTarget] };
    ID3D11UnorderedAccessView* pUAV[] = { m_DilateAO._uav };

    if (desc.m_MultiResLayerScale[selectTarget] < 1.0)
        pSRV[2] = m_ResultAO[selectTarget]._srv;

    // override defult behaviour if target == m_MultiResLayerCount
    // this indicates that all layers have already been dilated
    if (target == m_MultiResLayerCount)
    {
        pSRV[2] = m_DilateAO._srv;
        pUAV[0] = m_AO[selectTarget]._uav;
    }

    UINT uX, uY, uZ = 1;

    // Horizontal blur
    desc.m_pDeviceContext->CSSetConstantBuffers(0, AMD_ARRAY_SIZE(pCB), pCB);
    desc.m_pDeviceContext->CSSetUnorderedAccessViews(0, AMD_ARRAY_SIZE(pUAV), pUAV, NULL);
    desc.m_pDeviceContext->CSSetShaderResources(0, AMD_ARRAY_SIZE(pSRV), pSRV);

#if USE_NEW_BLUR_PROTOTYPE 
    desc.m_pDeviceContext->CSSetShader(m_csBilateralBlurHorizontal[desc.m_BilateralBlurRadius[selectTarget]], NULL, 0);
    uX = (int)ceilf((float)desc.m_InputSize.x / m_BilateralGroupDim);
    uY = (int)ceilf((float)desc.m_InputSize.y / m_BilateralGroupDim);
#else
    desc.m_pDeviceContext->CSSetShader(m_csBilateralBlurH[desc.m_BilateralBlurRadius[selectTarget]], NULL, 0);
    uX = (int)ceilf((float)desc.m_InputSize.x / m_BlurGroupSize);
    uY = (int)ceilf((float)desc.m_InputSize.y / m_BlurGroupLines);
#endif

    desc.m_pDeviceContext->Dispatch(uX, uY, uZ);
    desc.m_pDeviceContext->CSSetUnorderedAccessViews(0, AMD_ARRAY_SIZE(pNullUAV), pNullUAV, NULL);

    // Vertical pass
    pUAV[0] = m_AO[selectTarget]._uav;
    pSRV[2] = m_DilateAO._srv;

    // Again, override defult behaviour if target == m_MultiResLayerCount
    if (target == m_MultiResLayerCount)
    {
        pSRV[2] = m_AO[selectTarget]._srv;
        pUAV[0] = m_DilateAO._uav;
    }

    desc.m_pDeviceContext->CSSetUnorderedAccessViews(0, AMD_ARRAY_SIZE(pUAV), pUAV, NULL);
    desc.m_pDeviceContext->CSSetShaderResources(0, AMD_ARRAY_SIZE(pSRV), pSRV);
#if USE_NEW_BLUR_PROTOTYPE 
    desc.m_pDeviceContext->CSSetShader(m_csBilateralBlurVertical[desc.m_BilateralBlurRadius[selectTarget]], NULL, 0);
    uX = (int)ceilf((float)desc.m_InputSize.x / m_BilateralGroupDim);
    uY = (int)ceilf((float)desc.m_InputSize.y / m_BilateralGroupDim);
#else
    desc.m_pDeviceContext->CSSetShader(m_csBilateralBlurV[desc.m_BilateralBlurRadius[selectTarget]], NULL, 0);
    uX = (int)ceilf((float)desc.m_InputSize.x / m_BlurGroupLines);
    uY = (int)ceilf((float)desc.m_InputSize.y / m_BlurGroupSize);
#endif

    desc.m_pDeviceContext->Dispatch(uX, uY, uZ);
    desc.m_pDeviceContext->CSSetUnorderedAccessViews(0, AMD_ARRAY_SIZE(pNullUAV), pNullUAV, NULL);
    desc.m_pDeviceContext->CSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);

    return AOFX_RETURN_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
AOFX_RETURN_CODE  AOFX_OpaqueDesc::csBlur(uint target, const AOFX_Desc & desc)
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    AO_InputData aoInputData(desc, target);

    aoInputData.m_OutputSize.x = desc.m_InputSize.x;
    aoInputData.m_OutputSize.y = desc.m_InputSize.y;
    aoInputData.m_OutputSizeRcp.x = 1.0f / aoInputData.m_OutputSize.x;
    aoInputData.m_OutputSizeRcp.y = 1.0f / aoInputData.m_OutputSize.y;
    aoInputData.m_InputSize.x = desc.m_InputSize.x;
    aoInputData.m_InputSize.y = desc.m_InputSize.y;
    aoInputData.m_InputSizeRcp.x = 1.0f / aoInputData.m_InputSize.x;
    aoInputData.m_InputSizeRcp.y = 1.0f / aoInputData.m_InputSize.y;

    assert(sizeof(m_aoBilateralBlurData[target]) == sizeof(aoInputData));
    if (0 != memcmp(&m_aoBilateralBlurData[target], &aoInputData, sizeof(m_aoBilateralBlurData[target])))
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        desc.m_pDeviceContext->Map(m_cbBilateralBlurData[target], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        memcpy(mappedResource.pData, &aoInputData, sizeof(aoInputData));
        desc.m_pDeviceContext->Unmap(m_cbBilateralBlurData[target], 0);
        memcpy(&m_aoBilateralBlurData[target], &aoInputData, sizeof(aoInputData));
    }

    ID3D11SamplerState*        pSS[] = { m_ssPointClamp, m_ssLinearClamp };
    ID3D11ShaderResourceView*  pSRV[] = { desc.m_pDepthSRV, desc.m_pNormalSRV, m_AO[target]._srv };
    ID3D11Buffer*              pCB[] = { m_cbBilateralBlurData[target], m_cbSamplePatterns };
    ID3D11UnorderedAccessView* pUAV[] = { m_DilateAO._uav };

    if (desc.m_MultiResLayerScale[target] < 1.0)
        pSRV[2] = m_ResultAO[target]._srv;

    desc.m_pDeviceContext->CSSetSamplers(0, AMD_ARRAY_SIZE(pSS), pSS);
    desc.m_pDeviceContext->CSSetConstantBuffers(0, AMD_ARRAY_SIZE(pCB), pCB);
    desc.m_pDeviceContext->CSSetShaderResources(0, AMD_ARRAY_SIZE(pSRV), pSRV);
    desc.m_pDeviceContext->CSSetUnorderedAccessViews(0, AMD_ARRAY_SIZE(pUAV), pUAV, NULL);

    desc.m_pDeviceContext->CSSetShader(m_csBilateralBlurUpsampling[desc.m_BilateralBlurRadius[target]], NULL, 0);
    UINT uX = (int)ceilf((float)desc.m_InputSize.x / m_BilateralGroupDim);
    UINT uY = (int)ceilf((float)desc.m_InputSize.y / m_BilateralGroupDim);
    UINT uZ = 1;

    desc.m_pDeviceContext->Dispatch(uX, uY, uZ);

    ID3D11ShaderResourceView*  pNullSRV[8] = { 0 };
    ID3D11UnorderedAccessView* pNullUAV[8] = { 0 };
    desc.m_pDeviceContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, NULL);
    desc.m_pDeviceContext->CSSetShaderResources(0, 3, pNullSRV);

    return AOFX_RETURN_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------  
AOFX_RETURN_CODE  AOFX_OpaqueDesc::psDilateMultiResAO(const AOFX_Desc & desc)
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    CD3D11_VIEWPORT vpFullscreen(0.0f, 0.0f, (float)desc.m_InputSize.x, (float)desc.m_InputSize.y, 0.0f, 1.0f);

    ID3D11RenderTargetView*   pRTV[] = { m_DilateAO._rtv };
    ID3D11ShaderResourceView* pSRV[] = { m_AO[0]._srv, m_AO[1]._srv, m_AO[2]._srv,
      m_InputAO[0]._srv, m_InputAO[1]._srv, m_InputAO[2]._srv, desc.m_pDepthSRV };
    ID3D11SamplerState*       pSS[] = { m_ssPointClamp, m_ssLinearClamp };

    int active[3] = {
      desc.m_LayerProcess[0] != AOFX_LAYER_PROCESS_NONE,
      desc.m_LayerProcess[1] != AOFX_LAYER_PROCESS_NONE,
      desc.m_LayerProcess[2] != AOFX_LAYER_PROCESS_NONE
    };

    S_DILATE_DATA dilate_data;
    for (int i = 0; i != m_MultiResLayerCount; ++i)
    {
        dilate_data.m_PowIntensity.v[i] = desc.m_PowIntensity[i];
    }
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    desc.m_pDeviceContext->Map(m_cbDilateData, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    memcpy(mappedResource.pData, &dilate_data, sizeof(S_DILATE_DATA));
    desc.m_pDeviceContext->Unmap(m_cbDilateData, 0);
    
    HRESULT hr = AMD::RenderFullscreenPass(desc.m_pDeviceContext,
                                           vpFullscreen, m_vsFullscreen, m_psDilate[active[0]][active[1]][active[2]],
                                           NULL, 0, &m_cbDilateData, 1,
                                           pSS, AMD_ARRAY_SIZE(pSS),
                                           pSRV, AMD_ARRAY_SIZE(pSRV),
                                           pRTV, AMD_ARRAY_SIZE(pRTV),
                                           NULL, 0, 0,
                                           NULL, NULL, 0, m_bsOutputChannel[0xf],
                                           m_rsNoCulling);

    return (hr == S_OK) ? AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_FAIL;
}

//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
AOFX_RETURN_CODE  AOFX_OpaqueDesc::psUpsampleAO(uint target, const AOFX_Desc & desc)
{

    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    CD3D11_VIEWPORT vpFullscreen(0.0f, 0.0f, (float)desc.m_InputSize.x, (float)desc.m_InputSize.y, 0.0f, 1.0f);

    ID3D11Buffer*             pCB[] = { m_cbAOInputData[target] };
    ID3D11RenderTargetView*   pRTV[] = { m_AO[target]._rtv };
    ID3D11ShaderResourceView* pSRV[] = { desc.m_pDepthSRV, m_InputAO[target]._srv, m_ResultAO[target]._srv };
    ID3D11SamplerState*       pSS[] = { m_ssPointClamp, m_ssLinearClamp };

    HRESULT hr = AMD::RenderFullscreenPass(desc.m_pDeviceContext,
                              vpFullscreen, m_vsFullscreen, m_psUpscale,
                              NULL, 0,
                              pCB, AMD_ARRAY_SIZE(pCB),
                              pSS, AMD_ARRAY_SIZE(pSS),
                              pSRV, AMD_ARRAY_SIZE(pSRV),
                              pRTV, AMD_ARRAY_SIZE(pRTV),
                              NULL, 0, 0,
                              NULL, NULL, 0, m_bsOutputChannel[0xf],
                              m_rsNoCulling);
    
    return (hr == S_OK) ? AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_FAIL;
}



//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
AOFX_RETURN_CODE  AOFX_OpaqueDesc::render(const AOFX_Desc & desc)
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    bool disabled = true;
    for (int i = 0; i < m_MultiResLayerCount; i++)
        disabled = disabled && (desc.m_LayerProcess[i] == AOFX_LAYER_PROCESS_NONE);
    if (disabled) return AOFX_RETURN_CODE_SUCCESS;

    if (desc.m_pDeviceContext == NULL)
        return AOFX_RETURN_CODE_INVALID_DEVICE_CONTEXT;
    if (desc.m_pDepthSRV == NULL || 
        desc.m_pOutputRTV == NULL)
        return AOFX_RETURN_CODE_INVALID_POINTER;
    if (desc.m_InputSize.x == 0 || 
        desc.m_InputSize.y == 0)
        return AOFX_RETURN_CODE_INVALID_ARGUMENT;

    // Down sample depth and normals
    for (int i = 0; i < m_MultiResLayerCount; ++i)
    {
        if (desc.m_LayerProcess[i] == AOFX_LAYER_PROCESS_NONE) continue;
        if (desc.m_Implementation & AOFX_IMPLEMENTATION_MASK_UTILITY_PS)
            psProcessInput(i, desc);
        if (desc.m_Implementation & AOFX_IMPLEMENTATION_MASK_UTILITY_CS)
            csProcessInput(i, desc);
    }

    for (int i = 0; i < m_MultiResLayerCount; ++i)
    {
        if (desc.m_LayerProcess[i] == AOFX_LAYER_PROCESS_NONE) continue;
        if (desc.m_Implementation & AOFX_IMPLEMENTATION_MASK_KERNEL_CS)
            csAmbientOcclusion(i, desc);
        if (desc.m_Implementation & AOFX_IMPLEMENTATION_MASK_KERNEL_PS)
            psAmbientOcclusion(i, desc);

#if USE_NEW_BLUR_PROTOTYPE
        csBlur(i, desc);
#endif
    }

#if !USE_NEW_BLUR_PROTOTYPE
    // Need to check if all layers have the same blur radius (and that the blur radius != NONE
    bool separateBlur = false;
    bool active[m_MultiResLayerCount];
    int  blurRadius[m_MultiResLayerCount];
    int  blurRadiusResult = AOFX_BILATERAL_BLUR_RADIUS_NONE;

    for (int i = 0; i < m_MultiResLayerCount; ++i)
    {
        active[i] = desc.m_LayerProcess[i] != AOFX_LAYER_PROCESS_NONE;
        blurRadius[i] = active[i] ? desc.m_BilateralBlurRadius[i] : AOFX_BILATERAL_BLUR_RADIUS_NONE;
        blurRadiusResult = MAX(blurRadiusResult, blurRadius[i]);
    }
    for (int i = 0; i < m_MultiResLayerCount; ++i)
    {
        separateBlur = separateBlur || (active[i] && active[(i + 1) % m_MultiResLayerCount] && blurRadius[i] != blurRadius[(i + 1) % m_MultiResLayerCount]);
    }

    // Upsample all downscaled AO layers
    for (int i = 0; i < m_MultiResLayerCount; ++i)
    {
        if (desc.m_LayerProcess[i] != AOFX_LAYER_PROCESS_NONE &&
            desc.m_MultiResLayerScale[i] < 1.0f)
            psUpsampleAO(i, desc); // this should be a fancier upsampling kernel compared to current bilinear
    }

    // if each layer has a different blur radius, AO layers need to be blurred before blended 
    if (separateBlur == true)
    {
        for (int i = 0; i < m_MultiResLayerCount; ++i)
        {
            if (desc.m_LayerProcess[i] != AOFX_LAYER_PROCESS_NONE &&
                desc.m_BilateralBlurRadius[i] != AOFX_BILATERAL_BLUR_RADIUS_NONE)
                csBlurAO(i, desc);
        }
    }

    // blend AO layers together using dilate (min) filter
    psDilateMultiResAO(desc);

    // if all layers had the same blur radius, previous blur passes were skipped 
    // and the dilated image can be blurred just once
    if (separateBlur == false &&
        blurRadiusResult != AOFX_BILATERAL_BLUR_RADIUS_NONE)
    {
        csBlurAO(m_MultiResLayerCount, desc);
    }
#endif

    ID3D11BlendState * pOutputBS = desc.m_pOutputBS != NULL ? desc.m_pOutputBS : m_bsOutputChannel[desc.m_OutputChannelsFlag];
    CD3D11_VIEWPORT vpFullscreen(0.0f, 0.0f, (float)desc.m_InputSize.x, (float)desc.m_InputSize.y);
    ID3D11SamplerState* pSS[] = { m_ssPointClamp, m_ssLinearClamp };

    HRESULT hr = AMD::RenderFullscreenPass(desc.m_pDeviceContext,
                                           vpFullscreen, m_vsFullscreen, m_psOutput,
                                           NULL, 0, NULL, 0,
                                           pSS, AMD_ARRAY_SIZE(pSS),
                                           &m_DilateAO._srv, 1,
                                           (ID3D11RenderTargetView**)&desc.m_pOutputRTV, 1,
                                           NULL, 0, 0,
                                           NULL, NULL, 0, pOutputBS,
                                           m_rsNoCulling);

    return hr == S_OK ? AOFX_RETURN_CODE_SUCCESS : AOFX_RETURN_CODE_FAIL;
}

}