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

#ifndef __AMD_AOFX_H__
#define __AMD_AOFX_H__

#   define AMD_AOFX_VERSION_MAJOR             2
#   define AMD_AOFX_VERSION_MINOR             0

// default to static lib
#   ifndef AMD_AOFX_COMPILE_DYNAMIC_LIB
#       define AMD_AOFX_COMPILE_DYNAMIC_LIB   0
#   endif

#   if AMD_AOFX_COMPILE_DYNAMIC_LIB
#       ifdef AMD_DLL_EXPORTS
#           define AMD_AOFX_DLL_API __declspec(dllexport)
#       else // AMD_DLL_EXPORTS
#           define AMD_AOFX_DLL_API __declspec(dllimport)
#       endif // AMD_DLL_EXPORTS
#   else // AMD_AOFX_COMPILE_DYNAMIC_LIB
#       define AMD_AOFX_DLL_API
#   endif // AMD_AOFX_COMPILE_DYNAMIC_LIB

#include "AMD_Types.h"

#   if defined(DEBUG) || defined(_DEBUG)
#       define AMD_AOFX_DEBUG                 1 // Debugging functionality is currently disabled
#   endif

namespace AMD
{

enum AOFX_RETURN_CODE
{
    AOFX_RETURN_CODE_SUCCESS,
    AOFX_RETURN_CODE_FAIL,
    AOFX_RETURN_CODE_INVALID_DEVICE,
    AOFX_RETURN_CODE_INVALID_DEVICE_CONTEXT,
    AOFX_RETURN_CODE_INVALID_ARGUMENT,
    AOFX_RETURN_CODE_INVALID_POINTER,
    AOFX_RETURN_CODE_D3D11_CALL_FAILED,

    AOFX_RETURN_CODE_COUNT,
};

enum AOFX_SAMPLE_COUNT
{
    AOFX_SAMPLE_COUNT_LOW = 0,
    AOFX_SAMPLE_COUNT_MEDIUM = 1,
    AOFX_SAMPLE_COUNT_HIGH = 2,
    AOFX_SAMPLE_COUNT_ULTRA = 3,

    AOFX_SAMPLE_COUNT_COUNT = 4,
};

/* A newer version of bilateral blur is being developed

enum AOFX_BLUR_RADIUS
{
    AOFX_BLUR_RADIUS_2 = 0,
    AOFX_BLUR_RADIUS_4 = 1,
    AOFX_BLUR_RADIUS_8 = 2,
    AOFX_BLUR_RADIUS_16 = 3,
    AOFX_BLUR_RADIUS_32 = 4,

    AOFX_BLUR_RADIUS_NONE = -1,
    AOFX_BLUR_RADIUS_COUNT = 5,
};
*/ 

enum AOFX_BILATERAL_BLUR_RADIUS
{
    AOFX_BILATERAL_BLUR_RADIUS_2 = 0,
    AOFX_BILATERAL_BLUR_RADIUS_4 = 1,
    AOFX_BILATERAL_BLUR_RADIUS_8 = 2,
    AOFX_BILATERAL_BLUR_RADIUS_16 = 3,

    AOFX_BILATERAL_BLUR_RADIUS_NONE = -1,
    AOFX_BILATERAL_BLUR_RADIUS_COUNT = 4,
};

enum AOFX_NORMAL_OPTION
{
    AOFX_NORMAL_OPTION_NONE = 0,
    AOFX_NORMAL_OPTION_READ_FROM_SRV = 1,

    AOFX_NORMAL_OPTION_COUNT = 2,
};

enum AOFX_TAP_TYPE
{
    AOFX_TAP_TYPE_FIXED = 0,
    AOFX_TAP_TYPE_RANDOM_CB = 1,
    AOFX_TAP_TYPE_RANDOM_SRV = 2,

    AOFX_TAP_TYPE_COUNT = 3,
};

enum AOFX_IMPLEMENTATION_MASK
{
    AOFX_IMPLEMENTATION_MASK_KERNEL_CS = 1,
    AOFX_IMPLEMENTATION_MASK_KERNEL_PS = 2,

    AOFX_IMPLEMENTATION_MASK_UTILITY_CS = 4,
    AOFX_IMPLEMENTATION_MASK_UTILITY_PS = 8,

    AOFX_IMPLEMENTATION_MASK_BLUR_CS = 16,
    AOFX_IMPLEMENTATION_MASK_BLUR_PS = 32,

    AOFX_IMPLEMENTATION_COUNT = 2,
};

enum AOFX_OUTPUT_CHANNEL
{
    AOFX_OUTPUT_CHANNEL_R = 1,
    AOFX_OUTPUT_CHANNEL_G = 2,
    AOFX_OUTPUT_CHANNEL_B = 4,
    AOFX_OUTPUT_CHANNEL_A = 8,

    AOFX_OUTPUT_CHANNEL_COUNT = 16,
};

enum AOFX_MSAA_LEVEL
{
    AOFX_MSAA_LEVEL_1 = 0,

    AOFX_MSAA_LEVEL_COUNT = 1,
};

enum AOFX_LAYER_PROCESS
{
    AOFX_LAYER_PROCESS_DEINTERLEAVE_NONE = 0,
    AOFX_LAYER_PROCESS_DEINTERLEAVE_2 = 1,
    AOFX_LAYER_PROCESS_DEINTERLEAVE_4 = 2,
    AOFX_LAYER_PROCESS_DEINTERLEAVE_8 = 3,

    AOFX_LAYER_PROCESS_NONE = 0xFFFFFFFF,
    AOFX_LAYER_PROCESS_COUNT = 4,
};

struct AOFX_OpaqueDesc;

struct AOFX_Desc
{
    /**
    FX Modules share a variety of trivial types such as vectors and
    camera structures. These types are declared inside an FX descriptor
    in order to avoid any collisions between different modules or app types.
    */
#pragma warning(push)
#pragma warning(disable : 4201)        // suppress nameless struct/union level 4 warnings
    AMD_DECLARE_BASIC_VECTOR_TYPE;
#pragma warning(pop)
    AMD_DECLARE_CAMERA_TYPE;

    static const uint                   m_MultiResLayerCount = 3;

    AOFX_LAYER_PROCESS                  m_LayerProcess[m_MultiResLayerCount];
    AOFX_BILATERAL_BLUR_RADIUS          m_BilateralBlurRadius[m_MultiResLayerCount];
    AOFX_SAMPLE_COUNT                   m_SampleCount[m_MultiResLayerCount];
    AOFX_NORMAL_OPTION                  m_NormalOption[m_MultiResLayerCount];
    AOFX_TAP_TYPE                       m_TapType[m_MultiResLayerCount];
    float                               m_MultiResLayerScale[m_MultiResLayerCount];
    float                               m_PowIntensity[m_MultiResLayerCount];
    float                               m_RejectRadius[m_MultiResLayerCount];
    float                               m_AcceptRadius[m_MultiResLayerCount];
    float                               m_RecipFadeOutDist[m_MultiResLayerCount];
    float                               m_LinearIntensity[m_MultiResLayerCount];
    float                               m_NormalScale[m_MultiResLayerCount];
    float                               m_ViewDistanceDiscard[m_MultiResLayerCount];
    float                               m_ViewDistanceFade[m_MultiResLayerCount];
    float                               m_DepthUpsampleThreshold[m_MultiResLayerCount];

    uint                                m_Implementation;

    Camera                              m_Camera;

    ID3D11Device*                       m_pDevice;
    ID3D11DeviceContext*                m_pDeviceContext;

    ID3D11ShaderResourceView*           m_pDepthSRV;
    ID3D11ShaderResourceView*           m_pNormalSRV;
    ID3D11RenderTargetView*             m_pOutputRTV;

    uint2                               m_InputSize;

    uint                                m_OutputChannelsFlag;
    ID3D11BlendState*                   m_pOutputBS;

    AMD_AOFX_DLL_API                    AOFX_Desc();

    /**
    All AOFX_Desc objects have a pointer to a single instance of a AOFX_OpaqueDesc.
    */
    AOFX_OpaqueDesc *                   m_pOpaque;

private:
    /**
    Copy constructor and assign operator are declared but lack implementation.
    This is done on purpose in order to avoid invoking them by accident.
    */
    AMD_AOFX_DLL_API                    AOFX_Desc(const AOFX_Desc &);
    AMD_AOFX_DLL_API                    AOFX_Desc & operator= (const AOFX_Desc &);
};

extern "C"
{
    /**
    Initialize internal data inside AOFX_Desc
    Calling this function requires setting up m_pDevice member
    */
    AMD_AOFX_DLL_API AOFX_RETURN_CODE   AOFX_Initialize(const AOFX_Desc & desc);

    /**
    Execute AOFX rendering for a given AOFX_Desc parameters descriptior
    Calling this function requires setting up:
    * m_pDeviceContext must be set to a valid immediate context
    * m_pDepthSRV must be set to a valid shader resource view pointing to a depth buffer resource
    * m_InputSize must be set to the correct size of m_pDepthSRV resource
    * m_pOutputRTV must be set to a valid render target view pointint to a resource of m_InputSize size
    * m_Camera members have to be initialized with camera properties.
    * At least one element in m_LayerProcess[] array has to be different from AOFX_LAYER_PROCESS_NONE (otherwise all layers are disabled)
        options for a layer process include {using original input resources, deinterleaving input by a factor of {2, 4, 8} }
    Optional parameters are:
    * m_pNormalSRV - resource has to be of the same dimensions as m_pDepthSRV
    * m_OutputChannelsFlag - specify render terget view output mask. Default value is 0xF
    * m_pOutputBS - specify application desire blend state for output (a non NULL value will override m_OutputChannelsFlag)
    * m_Implementation - specify implementation mask to switch between pixel and compute shader code paths.
    Default value is set to execute all stages in compute.
    * For all active layers (layers that specify a value in m_LayerProcess[] that is different from AOFX_LAYER_PROCESS_NONE)
    application can override a variaty of options:
    ** m_BilateralBlurRadius - alternate between radius values of {0, 2, 4, 8, 16}
    ** m_SampleCount - alternate between sample counts of {8, 16, 24, 32}
    ** m_NormalOption - alternate between AO affects that {don't adjust reconstructed position along normal, take normal into account when reconstructing position}
    ** m_TapType - alternate between sampling pattern that is {fixed, random and fetched from constant buffer, random and fetched from shader resource view}
    ** m_MultiResLayerScale - setting this scaler in range from (0.0f, 1.0] will result in input Depth (and Normal) buffer being scalled before being processed
    ** m_PowIntensity - setting this scaler to a value > 0.0f will result in gamma correction for AO
    ** m_RejectRadius
    ** m_AcceptRadius
    ** m_RecipFadeOutDist
    ** m_LinearIntensity - setting this scaler to a value > 0.0f will result will bias the linear interpolation of AO from no occlusion to full occlusion
    ** m_NormalScale - setting this scaler to a value > 0.0 will define how much Normal buffer affects position reconstruction
    ** m_ViewDistanceDiscard - AO is not computed past this distance (value is set in camera space)
    ** m_ViewDistanceFade - AO start fading to 1.0 past this distance (value is set in camera space). Must be < m_ViewDistanceDiscard
    ** m_DepthUpsampleThreshold - used for bilateral upsampling (currently disabled)
    */
    AMD_AOFX_DLL_API AOFX_RETURN_CODE   AOFX_Render(const AOFX_Desc & desc);

    /**
    Resize internal texture resources inside AOFX_OpaqueDesc
    Calling this function requires setting up:
    * m_pDevice
    It will also take into account the state of the following members:
    * Active layers process (as defined by m_LayerProcess[])
    * m_MultiResLayerScale
    * m_InputSize
    * m_NormalOption
    */
    AMD_AOFX_DLL_API AOFX_RETURN_CODE   AOFX_Resize(const AOFX_Desc & desc);

    /**
    Release all internal data used by AOFX_OpaqueDesc
    */
    AMD_AOFX_DLL_API AOFX_RETURN_CODE   AOFX_Release(const AOFX_Desc & desc);

    /**
    This debugging code is currently disabled
    */
#   if defined(AMD_AOFX_DEBUG)
    AMD_AOFX_DLL_API AOFX_RETURN_CODE   AOFX_DebugSerialize(AOFX_Desc & desc, const char * params);
    AMD_AOFX_DLL_API AOFX_RETURN_CODE   AOFX_DebugDeserialize(AOFX_Desc & desc, const char * params,
                                                              ID3D11Texture2D**          ppT2D[],
                                                              ID3D11ShaderResourceView** ppSRV[]);
#   endif
}
}

#endif // __AMD_AOFX_H__