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

#include "AMD_AOFX_Common.hlsl"

#pragma warning( disable : 3571 )    // disable "pow(f,e) will not work for negative f" warnings

#ifndef AOFX_BLUR_RADIUS
# define AOFX_BLUR_RADIUS                        1
#endif

#define AO_BLUR_NON_SEPARABLE                    0
#define AO_BLUR_SEPARABLE_VERTICAL               1
#define AO_BLUR_SEPARABLE_HORIZONTAL             2

#ifndef AO_BLUR_SEPARABLE 
# define AO_BLUR_SEPARABLE                       AO_BLUR_NON_SEPARABLE
#endif
Texture2D<float>                                 g_t2dAO                       : register( t2 );

RWTexture2D<float>                               g_t2dOutput                   : register( u0 );

cbuffer                                          CB_INPUT_DATA                 : register( b0 )
{
    AO_InputData                                 g_cbInputData;
};

#if (AOFX_IMPLEMENTATION == AOFX_IMPLEMENTATION_CS)

# define AO_BLUR_GROUP_THREAD_DIM                32
# define AO_BLUR_GROUP_TEXEL_OVERLAP             (AO_BLUR_GROUP_THREAD_DIM / 2)
# define AO_BLUR_GROUP_THREAD_CACHE              (AO_BLUR_GROUP_THREAD_DIM + AO_BLUR_GROUP_TEXEL_OVERLAP * 2)

struct AO_Depth
{
  float                                          m_AO;
  float                                          m_Depth;
};

#if (AO_BLUR_SEPARABLE == AO_BLUR_NON_SEPARABLE)
groupshared AO_Depth                             g_SharedCache[AO_BLUR_GROUP_THREAD_CACHE][AO_BLUR_GROUP_THREAD_CACHE];
#elif (AO_BLUR_SEPARABLE == AO_BLUR_SEPARABLE_VERTICAL)
groupshared AO_Depth                             g_SharedCache[AO_BLUR_GROUP_THREAD_DIM * 3][AO_BLUR_GROUP_THREAD_DIM];
#else
groupshared AO_Depth                             g_SharedCache[AO_BLUR_GROUP_THREAD_DIM][AO_BLUR_GROUP_THREAD_DIM * 3];
#endif

AO_Depth loadDepthAndAmbientOcclusion(float2 uv)
{
  AO_Depth Out;
  Out.m_AO = g_t2dAO.SampleLevel(g_ssLinearClamp, uv, 0.0f);
  Out.m_Depth = loadCameraSpaceDepthT2D(uv, g_cbInputData.m_CameraQ, g_cbInputData.m_CameraQTimesZNear);

  return Out;
}

void   storeDepthAndAmbientOcclusionInCache(AO_Depth In, int2 cacheCoord)
{
  g_SharedCache[cacheCoord.y][cacheCoord.x] = In;
}

void   updateAmbientOcclusionInCache(float ao, int2 cacheCoord)
{
  g_SharedCache[cacheCoord.y][cacheCoord.x].m_AO = ao;
}

AO_Depth fetchDepthAndAmbientOcclusionFromCache(int2 screenCoord, int2 cacheCoord)
{
  return g_SharedCache[cacheCoord.y][cacheCoord.x];
}

#define DEPTH_SIGMA g_cbInputData.m_DepthUpsampleThreshold

float  kernelBilateralBlurVertical(float2 screenCoord, int2 cacheCoord)
{
  float centerDepth = fetchDepthAndAmbientOcclusionFromCache(screenCoord, cacheCoord).m_Depth;
  float centerAO = fetchDepthAndAmbientOcclusionFromCache(screenCoord, cacheCoord).m_AO;
  float filtered = 0.0f;
  float weightSum = 0.0f;
  float sigma = (AOFX_BLUR_RADIUS + 1.0f) / 2.0f;
  float sigmaSqr = 2.0f * sigma * sigma;
  float depthSigma = DEPTH_SIGMA ; //g_cbInputData.m_DepthUpsampleThreshold;

  [unroll]
  for (int i = (-AOFX_BLUR_RADIUS) + 1; i < AOFX_BLUR_RADIUS; i++)
  {
    float deltaDepth = centerDepth - fetchDepthAndAmbientOcclusionFromCache(screenCoord + int2(0, i), cacheCoord + int2(0, i)).m_Depth;
    float deltaAO = centerAO - fetchDepthAndAmbientOcclusionFromCache(screenCoord + int2(0, i), cacheCoord + int2(0, i)).m_AO;
    float weightGaussian = exp( -(i * i) / sigmaSqr ) * (abs(deltaDepth) < DEPTH_SIGMA ? 1.0f : 0.0f);
    float weightDepth = exp( -(deltaDepth * deltaDepth) / depthSigma );
    float weight = weightGaussian;
    filtered += fetchDepthAndAmbientOcclusionFromCache(screenCoord + int2(0, i), cacheCoord + int2(0, i)).m_AO * weight;
    weightSum += weight * weightDepth;
  }

  filtered /= weightSum;

  return filtered;
}

float  kernelBilateralBlurHorizontal(float2 screenCoord, int2 cacheCoord)
{
  float centerDepth = fetchDepthAndAmbientOcclusionFromCache(screenCoord, cacheCoord).m_Depth;
  float centerAO = fetchDepthAndAmbientOcclusionFromCache(screenCoord, cacheCoord).m_AO;
  float filtered = 0.0f;
  float weightSum = 0.0f;
  float sigma = (AOFX_BLUR_RADIUS + 1.0f) / 2.0f;
  float sigmaSqr = 2.0f * sigma * sigma;
  float depthSigma = DEPTH_SIGMA;

  [unroll]
  for (int i = (-AOFX_BLUR_RADIUS) + 1; i < AOFX_BLUR_RADIUS; i++)
  {
    float deltaDepth = centerDepth - fetchDepthAndAmbientOcclusionFromCache(screenCoord + int2(i, 0), cacheCoord + int2(i, 0)).m_Depth;
    float deltaAO = centerAO - fetchDepthAndAmbientOcclusionFromCache(screenCoord + int2(0, i), cacheCoord + int2(0, i)).m_AO;
    float weightGaussian = exp( -(i * i) / sigmaSqr ) * (abs(deltaDepth) < DEPTH_SIGMA ? 1.0f : 0.0f);
    float weightDepth = exp( -(deltaDepth * deltaDepth) / depthSigma );
    float weight = weightGaussian;
    filtered += fetchDepthAndAmbientOcclusionFromCache(screenCoord + int2(i, 0), cacheCoord + int2(i, 0)).m_AO * weight;
    weightSum += weight * weightDepth;
  }

  filtered /= weightSum;

  return filtered;
}

//==================================================================================================
// Experimental code
//==================================================================================================
[numthreads( AO_BLUR_GROUP_THREAD_DIM, AO_BLUR_GROUP_THREAD_DIM, 1 )]
void csBilateralBlur(uint3 groupIdx    : SV_GroupID,
                     uint3 threadIdx   : SV_GroupThreadID,
                     uint  threadIndex : SV_GroupIndex,
                     uint3 dispatchIdx : SV_DispatchThreadID )
{
  float2 screenCoord, screenCoord_left, screenCoord_right;
  int2 cacheCoord, cacheCoord_left, cacheCoord_right;
  AO_Depth aoDepth;

  float2 baseCoord = dispatchIdx.xy + float2(0.5f, 0.5f);

  screenCoord = baseCoord + float2(-AO_BLUR_GROUP_TEXEL_OVERLAP, -AO_BLUR_GROUP_TEXEL_OVERLAP);
  aoDepth = loadDepthAndAmbientOcclusion(screenCoord * g_cbInputData.m_OutputSizeRcp);

  storeDepthAndAmbientOcclusionInCache(aoDepth, threadIdx.xy + int2(0, 0));

  screenCoord = baseCoord + float2(-AO_BLUR_GROUP_TEXEL_OVERLAP, -AO_BLUR_GROUP_TEXEL_OVERLAP + AO_BLUR_GROUP_THREAD_DIM);
  aoDepth = loadDepthAndAmbientOcclusion(screenCoord * g_cbInputData.m_OutputSizeRcp);

  storeDepthAndAmbientOcclusionInCache(aoDepth, threadIdx.xy + int2(0, AO_BLUR_GROUP_THREAD_DIM));

  screenCoord = baseCoord + float2(-AO_BLUR_GROUP_TEXEL_OVERLAP + AO_BLUR_GROUP_THREAD_DIM, -AO_BLUR_GROUP_TEXEL_OVERLAP);
  aoDepth = loadDepthAndAmbientOcclusion(screenCoord * g_cbInputData.m_OutputSizeRcp);

  storeDepthAndAmbientOcclusionInCache(aoDepth, threadIdx.xy + int2(AO_BLUR_GROUP_THREAD_DIM, 0));

  screenCoord = baseCoord + float2(-AO_BLUR_GROUP_TEXEL_OVERLAP + AO_BLUR_GROUP_THREAD_DIM, -AO_BLUR_GROUP_TEXEL_OVERLAP + AO_BLUR_GROUP_THREAD_DIM);
  aoDepth = loadDepthAndAmbientOcclusion(screenCoord * g_cbInputData.m_OutputSizeRcp);

  storeDepthAndAmbientOcclusionInCache(aoDepth, threadIdx.xy + int2(AO_BLUR_GROUP_THREAD_DIM, AO_BLUR_GROUP_THREAD_DIM));

  GroupMemoryBarrierWithGroupSync();

  int2 cachedCoord1 = threadIdx.xy + int2(0, AO_BLUR_GROUP_TEXEL_OVERLAP) ;
  float aoFiltered1 = kernelBilateralBlurVertical(float2(0,0), cachedCoord1);

  int2 cachedCoord2 = threadIdx.xy + int2(AO_BLUR_GROUP_THREAD_DIM, AO_BLUR_GROUP_TEXEL_OVERLAP) ;
  float aoFiltered2 = kernelBilateralBlurVertical(float2(0,0), cachedCoord2);

  GroupMemoryBarrierWithGroupSync();

  updateAmbientOcclusionInCache(aoFiltered1, cachedCoord1);
  updateAmbientOcclusionInCache(aoFiltered2, cachedCoord2);

  GroupMemoryBarrierWithGroupSync();

  int2 cachedCoord = threadIdx.xy + int2(AO_BLUR_GROUP_TEXEL_OVERLAP, AO_BLUR_GROUP_TEXEL_OVERLAP) ;
  float aoFiltered = kernelBilateralBlurHorizontal(float2(0,0), cachedCoord);

  if ( (dispatchIdx.x < g_cbInputData.m_OutputSize.x) && (dispatchIdx.y < g_cbInputData.m_OutputSize.y) )
  {
    g_t2dOutput[dispatchIdx.xy] = aoFiltered; 
  }
}

//==================================================================================================
// Experimental code
//==================================================================================================
[numthreads( AO_BLUR_GROUP_THREAD_DIM, AO_BLUR_GROUP_THREAD_DIM, 1 )]
void csBilateralBlurVertical(uint3 groupIdx    : SV_GroupID,
                             uint3 threadIdx   : SV_GroupThreadID,
                             uint  threadIndex : SV_GroupIndex,
                             uint3 dispatchIdx : SV_DispatchThreadID )
{
  float2 screenCoord, screenCoord_left, screenCoord_right;
  int2 cacheCoord, cacheCoord_left, cacheCoord_right;
  AO_Depth aoDepth;

  float2 baseCoord = dispatchIdx.xy + float2(0.5f, 0.5f);

  int cacheOffset = 0;

  for (int baseOffset = -AOFX_BLUR_RADIUS; baseOffset <= AOFX_BLUR_RADIUS + AO_BLUR_GROUP_THREAD_DIM; baseOffset += AO_BLUR_GROUP_THREAD_DIM)
  {
    screenCoord = baseCoord + float2(0, baseOffset);
    aoDepth = loadDepthAndAmbientOcclusion(screenCoord * g_cbInputData.m_OutputSizeRcp);
    storeDepthAndAmbientOcclusionInCache(aoDepth, threadIdx.xy + int2(0, cacheOffset));

    cacheOffset += AO_BLUR_GROUP_THREAD_DIM;
  }

  GroupMemoryBarrierWithGroupSync();

  int2 cachedCoord = threadIdx.xy + int2(0, AOFX_BLUR_RADIUS) ;

  if ( (dispatchIdx.x < g_cbInputData.m_OutputSize.x) && (dispatchIdx.y < g_cbInputData.m_OutputSize.y) )
  {
    float aoFiltered = kernelBilateralBlurVertical(float2(0,0), cachedCoord);
    g_t2dOutput[dispatchIdx.xy] = aoFiltered;
  }
}

//==================================================================================================
// Experimental code
//==================================================================================================
[numthreads( AO_BLUR_GROUP_THREAD_DIM, AO_BLUR_GROUP_THREAD_DIM, 1 )]
void csBilateralBlurHorizontal(uint3 groupIdx    : SV_GroupID,
                               uint3 threadIdx   : SV_GroupThreadID,
                               uint  threadIndex : SV_GroupIndex,
                               uint3 dispatchIdx : SV_DispatchThreadID )
{
  float2 screenCoord, screenCoord_left, screenCoord_right;
  int2 cacheCoord, cacheCoord_left, cacheCoord_right;
  AO_Depth aoDepth;

  float2 baseCoord = dispatchIdx.xy + float2(0.5f, 0.5f);

  int cacheOffset = 0;

  for (int baseOffset = -AOFX_BLUR_RADIUS; baseOffset <= AOFX_BLUR_RADIUS + AO_BLUR_GROUP_THREAD_DIM; baseOffset += AO_BLUR_GROUP_THREAD_DIM)
  {
    screenCoord = baseCoord + float2(baseOffset, 0);
    aoDepth = loadDepthAndAmbientOcclusion(screenCoord * g_cbInputData.m_OutputSizeRcp);
    storeDepthAndAmbientOcclusionInCache(aoDepth, threadIdx.xy + int2(cacheOffset, 0));

    cacheOffset += AO_BLUR_GROUP_THREAD_DIM;
  }

  GroupMemoryBarrierWithGroupSync();

  int2 cachedCoord = threadIdx.xy + int2(AOFX_BLUR_RADIUS, 0) ;
  float aoFiltered = kernelBilateralBlurHorizontal(float2(0,0), cachedCoord);

  if ( (dispatchIdx.x < g_cbInputData.m_OutputSize.x) && (dispatchIdx.y < g_cbInputData.m_OutputSize.y) )
  {
    g_t2dOutput[dispatchIdx.xy] = aoFiltered; 
  }
}


#elif (AOFX_IMPLEMENTATION == AOFX_IMPLEMENTATION_PS)

#include "../../../AMD_LIB/src/Shaders/AMD_FullscreenPass.hlsl"

Texture2D                                        g_t2dUpsampleDepth            : register( t0 );
Texture2D                                        g_t2dUpsampleDepthSmall  : register( t1 );
Texture2D                                        g_t2dUpsampleAO     : register( t2 );

//==================================================================================================
// Upsample PS: This should really do something more fancy than a bilinear upsample
// A depth guided upsampling can be uncommented for comparison, yet that doesn't always work well
//==================================================================================================
float4 psUpsample( PS_FullscreenInput In ) : SV_Target0
{
    float scaled_ao = 0.0f;

#if 0
    float4 scaled_z = g_t2dUpsampleDepthSmall.GatherRed( g_ssLinearClamp, In.texCoord, 0 );
    float  full_z = g_t2dUpsampleDepth.Gather( g_ssPointClamp, In.texCoord, 0 ).xyzw;

    full_z = -g_cbInputData.m_CameraQTimesZNear / ( full_z - g_cbInputData.m_CameraQ );

    float4 delta;
    delta.x = abs(scaled_z.x - full_z);
    delta.y = abs(scaled_z.y - full_z);
    delta.z = abs(scaled_z.z - full_z);
    delta.w = abs(scaled_z.w - full_z);

    float scale = ((float)g_cbInputData.m_InputSize.x) / ((float)g_cbInputData.m_OutputSize.x);
    if (scale <= 1.0001f) scale = 1000.0f;

    float2 low_resolution_base_uv = In.texCoord - 0.5 * g_cbInputData.m_OutputSizeRcp;

    float min_delta = delta.w;
    float2 ao_low_resolution_uv = low_resolution_base_uv;

    if (min_delta > delta.z)
    {
        min_delta = delta.z;
        ao_low_resolution_uv = float2(low_resolution_base_uv.x + g_cbInputData.m_OutputSizeRcp.x, low_resolution_base_uv.y);
    }

    if (min_delta > delta.x)
    {
        min_delta = delta.x;
        ao_low_resolution_uv = float2(low_resolution_base_uv.x, low_resolution_base_uv.y + g_cbInputData.m_OutputSizeRcp.y);
    }

    if (min_delta > delta.y)
    {
        min_delta = delta.y;
        ao_low_resolution_uv = low_resolution_base_uv + g_cbInputData.m_OutputSizeRcp;
    }

    if (delta.x <= g_cbInputData.m_DepthUpsampleThreshold &&
        delta.y <= g_cbInputData.m_DepthUpsampleThreshold &&
        delta.z <= g_cbInputData.m_DepthUpsampleThreshold &&
        delta.w <= g_cbInputData.m_DepthUpsampleThreshold) 
    {
        scaled_ao = g_t2dUpsampleAO.SampleLevel( g_ssPointClamp, In.texCoord, 0 ).x;
    }
    else
    {
        scaled_ao = g_t2dUpsampleAO.SampleLevel( g_ssPointClamp, ao_low_resolution_uv, 0 ).x;
    }
#else // depth guided upsample doesn't work very well, so for now just using simple bilinear
    scaled_ao = g_t2dUpsampleAO.SampleLevel( g_ssPointClamp, In.texCoord, 0 ).x;
#endif

    return scaled_ao;
}


//=================================================================================================================================
// This pixel shader implements a dilate operation over 3 AO surfaces with gamma correction
//=================================================================================================================================
Texture2D                                        g_t2dDilateAO0                      : register( t0 );
Texture2D                                        g_t2dDilateAO1                      : register( t1 );
Texture2D                                        g_t2dDilateAO2                      : register( t2 );

struct                                           DilateData
{
  float4                                         m_MultiResolutionAOFactor;
};

cbuffer                                          CB_DILATE_Data : register( b0 )
{
    DilateData                                   g_DilateData;
}

float4 psDilate( PS_FullscreenInput In ) : SV_Target0
{
    float3 ao = float3(1, 1, 1);
#if (AO_LAYER_MASK & 1) != 0
    ao.x = pow(g_t2dDilateAO0.SampleLevel( g_ssPointClamp, In.texCoord, 0 ).x, g_DilateData.m_MultiResolutionAOFactor.x);
#endif

#if (AO_LAYER_MASK & 2) != 0
    ao.y = pow(g_t2dDilateAO1.SampleLevel( g_ssPointClamp, In.texCoord, 0 ).x, g_DilateData.m_MultiResolutionAOFactor.y);
#endif

#if (AO_LAYER_MASK & 4) != 0
    ao.z = pow(g_t2dDilateAO2.SampleLevel( g_ssPointClamp, In.texCoord, 0 ).x, g_DilateData.m_MultiResolutionAOFactor.z);
#endif

    return min(ao.x, min(ao.y, ao.z));
}

//=================================================================================================================================
// This pixel shader implements a pass through copy of RED CHANNEL into RGBA OUTPUT
//=================================================================================================================================
Texture2D                                        g_t2dOutputRed                : register( t0 );

float4 psOutputRed( PS_FullscreenInput In ) : SV_Target
{
    float red = g_t2dOutputRed.Sample( g_ssLinearClamp, In.texCoord, 0 ).r;

    return float4(red, red, red, red);
}

#endif // (AOFX_IMPLEMENTATION == AOFX_IMPLEMENTATION_CS)


//=================================================================================================================================
// EOF
//=================================================================================================================================
