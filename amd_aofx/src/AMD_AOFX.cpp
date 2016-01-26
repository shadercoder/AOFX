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

#include <d3d11.h>
#include <string>
#include <iostream>
#include <fstream>

#define _USE_MATH_DEFINES
#include <cmath>

#if AMD_AOFX_COMPILE_DYNAMIC_LIB
# define AMD_DLL_EXPORTS
#endif

#include "AMD_AOFX_OPAQUE.h"

namespace AMD
{

//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
AOFX_RETURN_CODE AMD_AOFX_DLL_API AOFX_Initialize(const AOFX_Desc & desc)
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    AOFX_RETURN_CODE result = AOFX_RETURN_CODE_SUCCESS;

    if (NULL == desc.m_pDevice)
    {
        return AOFX_RETURN_CODE_INVALID_DEVICE;
    }

    result = desc.m_pOpaque->cbInitialize(desc);
    if (result != AOFX_RETURN_CODE_SUCCESS)
        return result;

    result = desc.m_pOpaque->createShaders(desc);

    return result;
}

//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
AOFX_RETURN_CODE AMD_AOFX_DLL_API AOFX_Render(const AOFX_Desc & desc)
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    if (NULL == desc.m_pDeviceContext)
    {
        return AOFX_RETURN_CODE_INVALID_DEVICE_CONTEXT;
    }

    AMD::C_SaveRestore_IA save_ia(desc.m_pDeviceContext);
    AMD::C_SaveRestore_VS save_vs(desc.m_pDeviceContext);
    AMD::C_SaveRestore_HS save_hs(desc.m_pDeviceContext);
    AMD::C_SaveRestore_DS save_ds(desc.m_pDeviceContext);
    AMD::C_SaveRestore_GS save_gs(desc.m_pDeviceContext);
    AMD::C_SaveRestore_PS save_ps(desc.m_pDeviceContext);
    AMD::C_SaveRestore_RS save_rs(desc.m_pDeviceContext);
    AMD::C_SaveRestore_OM save_om(desc.m_pDeviceContext);
    AMD::C_SaveRestore_CS save_cs(desc.m_pDeviceContext);

    AOFX_RETURN_CODE result = desc.m_pOpaque->render(desc);

    return result;
}

//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
AOFX_RETURN_CODE AMD_AOFX_DLL_API AOFX_Resize(const AOFX_Desc & desc)
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    if (NULL == desc.m_pDevice)
    {
        return AOFX_RETURN_CODE_INVALID_DEVICE;
    }
    if (NULL == desc.m_pDeviceContext)
    {
        return AOFX_RETURN_CODE_INVALID_DEVICE_CONTEXT;
    }

    AOFX_RETURN_CODE result = desc.m_pOpaque->resize(desc);

    return result;
}

//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
AOFX_RETURN_CODE AMD_AOFX_DLL_API AOFX_Release(const AOFX_Desc & desc)
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME " \n");

    desc.m_pOpaque->release();

    return AOFX_RETURN_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
AOFX_Desc::AOFX_Desc()
    : m_Implementation(AOFX_IMPLEMENTATION_MASK_KERNEL_CS | AOFX_IMPLEMENTATION_MASK_BLUR_CS | AOFX_IMPLEMENTATION_MASK_UTILITY_CS)
    , m_pDeviceContext(NULL)
    , m_pNormalSRV(NULL)
    , m_pDepthSRV(NULL)
    , m_pDevice(NULL)
    , m_pOutputRTV(NULL)
    , m_pOpaque(NULL)
    , m_OutputChannelsFlag(0xF)
    , m_pOutputBS(NULL)
{
    AMD_OUTPUT_DEBUG_STRING("CALL: " AMD_FUNCTION_NAME "\n");

    static AOFX_OpaqueDesc opaque(*this);

    for (AMD::uint i = 0; i < m_MultiResLayerCount; i++)
    {
        m_LayerProcess[i] = AOFX_LAYER_PROCESS_DEINTERLEAVE_NONE;
        m_MultiResLayerScale[i] = 1.0f / powf(2.0f, (float)i);
        m_PowIntensity[i] = 1.0f;
        m_BilateralBlurRadius[i] = AOFX_BILATERAL_BLUR_RADIUS_NONE;
        m_ViewDistanceDiscard[i] = 100.0f;
        m_ViewDistanceFade[i] = 99.0f;
        m_NormalScale[i] = 0.1f;
        m_LinearIntensity[i] = 0.6f;
        m_AcceptRadius[i] = 0.003f;
        m_RejectRadius[i] = 0.8f;
        m_RecipFadeOutDist[i] = 6.0f;
        m_DepthUpsampleThreshold[i] = 0.05f;

        m_SampleCount[i] = AOFX_SAMPLE_COUNT_LOW;
        m_NormalOption[i] = AOFX_NORMAL_OPTION_NONE;
        m_TapType[i] = AOFX_TAP_TYPE_FIXED;
    }

    m_pOpaque = &opaque;
}
}