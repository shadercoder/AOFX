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

#ifndef __AMD_AOFX_OPAQUE_DESC_H__
#define __AMD_AOFX_OPAQUE_DESC_H__

#include "AMD_LIB.h"
#include "AMD_AOFX.h"

#include <math.h>

#pragma warning( disable : 4127 ) // disable conditional expression is constant warnings

#define AO_DEINTERLEAVE_OUTPUT_TO_TEXTURE2DARRAY 0

namespace AMD
{
struct AOFX_OpaqueDesc
{
public:

#pragma warning(push)
#pragma warning(disable : 4201)        // suppress nameless struct/union level 4 warnings
    AMD_DECLARE_BASIC_VECTOR_TYPE;
#pragma warning(pop)

    // Constant buffer layout for transferring data to the AO shaders
    template <class T, DXGI_FORMAT dxgiFormat>
    struct T_CB_SAMPLEPATTERN_ROT
    {
        static const uint        numRotations = 64;
        static const uint        numSamplePatterns = 32;
        static const DXGI_FORMAT format = dxgiFormat;

        T SP[numRotations][numSamplePatterns];
    };

    // constant buffer requires 16 byte alignement of elements
    typedef T_CB_SAMPLEPATTERN_ROT<float4, DXGI_FORMAT_R32G32B32A32_FLOAT> CB_SAMPLEPATTERN_ROT_FLOAT4;
    typedef T_CB_SAMPLEPATTERN_ROT<sint4, DXGI_FORMAT_R32G32B32A32_SINT>  CB_SAMPLEPATTERN_ROT_SINT4;
    typedef T_CB_SAMPLEPATTERN_ROT<float2, DXGI_FORMAT_R32G32_FLOAT>       CB_SAMPLEPATTERN_ROT_FLOAT2;
    typedef T_CB_SAMPLEPATTERN_ROT<sint2, DXGI_FORMAT_R32G32_SINT>        CB_SAMPLEPATTERN_ROT_SINT2;
    typedef T_CB_SAMPLEPATTERN_ROT<sshort2, DXGI_FORMAT_R16G16_SINT>        CB_SAMPLEPATTERN_ROT_SSHORT2;
    typedef T_CB_SAMPLEPATTERN_ROT<sbyte2, DXGI_FORMAT_R8G8_SINT>          CB_SAMPLEPATTERN_ROT_SBYTE2;

    static const sint m_DeinterleaveSize[AOFX_LAYER_PROCESS_COUNT];

    static const sint m_MultiResLayerCount = AOFX_Desc::m_MultiResLayerCount;

    // CS params
    static const uint m_AOGroupDim = 32;
    static const uint m_DeinterleaveGroupDim = 32;
    static const uint m_BilateralGroupDim = 32;
    static const uint m_BlurGroupSize = 128;
    static const uint m_BlurGroupLines = 2;

    // Constant buffer layout for transferring data to the AO shaders
    struct AO_Data
    {
        uint2                                 m_OutputSize;
        float2                                m_OutputSizeRcp;
        uint2                                 m_InputSize;                  // size (xy), inv size (zw)
        float2                                m_InputSizeRcp;               // size (xy), inv size (zw)

        float                                 m_CameraQ;                    // far / (far - near)
        float                                 m_CameraQTimesZNear;          // Q * near
        float                                 m_CameraTanHalfFovHorizontal; // Tan Horiz and Vert FOV
        float                                 m_CameraTanHalfFovVertical;

        float                                 m_RejectRadius;
        float                                 m_AcceptRadius;
        float                                 m_RecipFadeOutDist;
        float                                 m_LinearIntensity;

        float                                 m_NormalScale;
        float                                 m_MultiResLayerScale;
        float                                 m_ViewDistanceFade;
        float                                 m_ViewDistanceDiscrad;

        float                                 m_FadeIntervalLength;
        float                                 _pad[3];

        AO_Data(const AOFX_Desc & desc, unsigned int target)
        {
            float zDistance = (desc.m_Camera.m_FarPlane - desc.m_Camera.m_NearPlane);
            this->m_CameraQ = desc.m_Camera.m_FarPlane / (zDistance); // camera_far_clip / ( camera_far_clip - camera_near_clip );
            this->m_CameraQTimesZNear = this->m_CameraQ * desc.m_Camera.m_NearPlane;                                   // cameraQ * camera_near_clip;
            this->m_CameraTanHalfFovHorizontal = tanf(desc.m_Camera.m_Fov * 0.5f * desc.m_Camera.m_Aspect);
            this->m_CameraTanHalfFovVertical = tanf(desc.m_Camera.m_Fov * 0.5f);
            this->m_FadeIntervalLength = desc.m_ViewDistanceDiscard[target] - desc.m_ViewDistanceFade[target];
            this->m_ViewDistanceFade = desc.m_ViewDistanceFade[target];
            this->m_ViewDistanceDiscrad = desc.m_ViewDistanceDiscard[target];
            this->m_AcceptRadius = desc.m_AcceptRadius[target];
            this->m_LinearIntensity = desc.m_LinearIntensity[target];
            this->m_NormalScale = desc.m_NormalScale[target];
            this->m_RecipFadeOutDist = desc.m_RecipFadeOutDist[target];
            this->m_RejectRadius = desc.m_RejectRadius[target];
            this->m_MultiResLayerScale = desc.m_MultiResLayerScale[target];
        }

        AO_Data()
        {
            memset(this, 0, sizeof(AO_Data));
        }
    };

    struct AO_InputData
    {
        uint2                                 m_OutputSize;
        float2                                m_OutputSizeRcp;
        uint2                                 m_InputSize;
        float2                                m_InputSizeRcp;

        float                                 m_ZFar;
        float                                 m_ZNear;
        float                                 m_CameraQ;                    // far / (far - near)
        float                                 m_CameraQTimesZNear;          // cameraQ * near
        float                                 m_CameraTanHalfFovHorizontal; // Tan Horiz and Vert FOV
        float                                 m_CameraTanHalfFovVertical;

        float                                 m_DepthUpsampleThreshold;
        float                                 m_NormalScale;

        float                                 m_Scale;
        float                                 m_ScaleRcp;
        float2                                _pad;

        AO_InputData(const AOFX_Desc & desc, unsigned int target)
        {
            this->m_ZFar = desc.m_Camera.m_FarPlane;
            this->m_ZNear = desc.m_Camera.m_NearPlane;
            float zDistance = (desc.m_Camera.m_FarPlane - desc.m_Camera.m_NearPlane);
            this->m_CameraQ = desc.m_Camera.m_FarPlane / zDistance;
            this->m_CameraQTimesZNear = this->m_CameraQ * desc.m_Camera.m_NearPlane;
            this->m_NormalScale = desc.m_NormalScale[target];
            this->m_CameraTanHalfFovHorizontal = tanf(desc.m_Camera.m_Fov * 0.5f * desc.m_Camera.m_Aspect);
            this->m_CameraTanHalfFovVertical = tanf(desc.m_Camera.m_Fov * 0.5f);
            this->m_Scale = desc.m_MultiResLayerScale[target];
            this->m_ScaleRcp = 1.0f / desc.m_MultiResLayerScale[target];
            this->m_DepthUpsampleThreshold = desc.m_DepthUpsampleThreshold[target];
        }

        AO_InputData()
        {
            memset(this, 0, sizeof(AO_InputData));
        }
    };

    struct CB_BILATERAL_DILATE
    {
        float4                                m_OutputSize;             // Back buffer size (xy), inv size (zw)
        float                                 m_CameraQ;                // far / (far - near)
        float                                 m_CameraQTimesZNear;      // Q * near
        float                                 _pad[2];
    };

    struct S_DILATE_DATA
    {
        float4                                m_PowIntensity;
    };

    // these members store current AO state and are used to optimize constant buffer updates 
    // and AO surface resize routine
    uint2                                   m_Resolution;

    AO_Data                                 m_aoData[m_MultiResLayerCount];
    AO_InputData                            m_aoInputData[m_MultiResLayerCount];
    AO_InputData                            m_aoBilateralBlurData[m_MultiResLayerCount];
    AOFX_LAYER_PROCESS                      m_LayerProcess[m_MultiResLayerCount];
    AOFX_NORMAL_OPTION                      m_NormalOption[m_MultiResLayerCount];
    uint2                                   m_ScaledResolution[m_MultiResLayerCount];

    AMD::Texture2D                          m_DilateAO;
    AMD::Texture2D                          m_AO[m_MultiResLayerCount];
    AMD::Texture2D                          m_ResultAO[m_MultiResLayerCount];
    AMD::Texture2D                          m_InputAO[m_MultiResLayerCount];

    DXGI_FORMAT                             m_FormatAO;
    DXGI_FORMAT                             m_FormatDepthNormal;
    DXGI_FORMAT                             m_FormatDepth;

    ID3D11VertexShader*                     m_vsFullscreen;

    ID3D11PixelShader*                      m_psUpscale;

    ID3D11ComputeShader*                    m_csProcessInput[AOFX_LAYER_PROCESS_COUNT][AOFX_NORMAL_OPTION_COUNT][AOFX_MSAA_LEVEL_COUNT];
    ID3D11PixelShader*                      m_psProcessInput[AOFX_LAYER_PROCESS_COUNT][AOFX_NORMAL_OPTION_COUNT][AOFX_MSAA_LEVEL_COUNT];
    ID3D11ComputeShader*                    m_csAmbientOcclusion[AOFX_LAYER_PROCESS_COUNT][AOFX_NORMAL_OPTION_COUNT][AOFX_TAP_TYPE_COUNT][AOFX_SAMPLE_COUNT_COUNT];
    ID3D11PixelShader*                      m_psAmbientOcclusion[AOFX_LAYER_PROCESS_COUNT][AOFX_NORMAL_OPTION_COUNT][AOFX_TAP_TYPE_COUNT][AOFX_SAMPLE_COUNT_COUNT];

    // new "prototype" implementation (this is not currently being used by default)
    ID3D11ComputeShader*                    m_csBilateralBlurUpsampling[AOFX_BILATERAL_BLUR_RADIUS_COUNT];
    ID3D11ComputeShader*                    m_csBilateralBlurHorizontal[AOFX_BILATERAL_BLUR_RADIUS_COUNT];
    ID3D11ComputeShader*                    m_csBilateralBlurVertical[AOFX_BILATERAL_BLUR_RADIUS_COUNT];
    ID3D11PixelShader*                      m_psBilateralBlurUpsamplingH[AOFX_BILATERAL_BLUR_RADIUS_COUNT];
    ID3D11PixelShader*                      m_psBilateralBlurUpsamplingV[AOFX_BILATERAL_BLUR_RADIUS_COUNT];

    // older implementation
    ID3D11ComputeShader*                    m_csBilateralBlurH[AOFX_BILATERAL_BLUR_RADIUS_COUNT];
    ID3D11ComputeShader*                    m_csBilateralBlurV[AOFX_BILATERAL_BLUR_RADIUS_COUNT];

    ID3D11PixelShader*                      m_psDilate[2][2][2];

    ID3D11PixelShader*                      m_psOutput;

    // sample pattern buffer declaration
    ID3D11Buffer*                           m_cbSamplePatterns;
    ID3D11Buffer*                           m_tbSamplePatterns;
    ID3D11ShaderResourceView*               m_tbSamplePatternsSRV;

    // Various Constant buffers
    ID3D11Buffer*                           m_cbAOData[m_MultiResLayerCount];
    ID3D11Buffer*                           m_cbAOInputData[m_MultiResLayerCount];
    ID3D11Buffer*                           m_cbBilateralBlurData[m_MultiResLayerCount];
    ID3D11Buffer*                           m_cbBilateralDilate;
    ID3D11Buffer*                           m_cbDilateData;

    ID3D11SamplerState*                     m_ssPointClamp;
    ID3D11SamplerState*                     m_ssLinearClamp;
    ID3D11SamplerState*                     m_ssPointWrap;
    ID3D11SamplerState*                     m_ssLinearWrap;

    ID3D11RasterizerState*                  m_rsNoCulling;
    ID3D11BlendState*                       m_bsOutputChannel[AOFX_OUTPUT_CHANNEL_COUNT];

    ~AOFX_OpaqueDesc();
    AOFX_OpaqueDesc(const AOFX_Desc & desc);

    AOFX_RETURN_CODE                        cbInitialize(const AOFX_Desc & desc);
    AOFX_RETURN_CODE                        resize(const AOFX_Desc & desc);

    AOFX_RETURN_CODE                        render(const AOFX_Desc & desc);

    AOFX_RETURN_CODE                        csProcessInput(uint target, const AOFX_Desc & desc);
    AOFX_RETURN_CODE                        psProcessInput(uint target, const AOFX_Desc & desc);

    AOFX_RETURN_CODE                        psAmbientOcclusion(uint target, const AOFX_Desc & desc);
    AOFX_RETURN_CODE                        csAmbientOcclusion(uint target, const AOFX_Desc & desc);

    AOFX_RETURN_CODE                        psBlur(uint target, const AOFX_Desc & desc);
    AOFX_RETURN_CODE                        csBlur(uint target, const AOFX_Desc & desc);

    AOFX_RETURN_CODE                        psUpsampleAO(uint target, const AOFX_Desc & desc);

    AOFX_RETURN_CODE                        csBlurAO(uint target, const AOFX_Desc & desc);
    AOFX_RETURN_CODE                        psDilateMultiResAO(const AOFX_Desc & desc);

    AOFX_RETURN_CODE                        createShaders(const AOFX_Desc & desc);

    void                                    release();
    void                                    releaseShaders();
    void                                    releaseTextures();
};

} // namespace AMD

#endif // __AMD_AOFX_OpaqueDesc_H__
