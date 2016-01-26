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

//==================================================================================================
// Thread / Group Defines
//==================================================================================================

#include "AMD_AOFX_Common.hlsl"

#define AO_DEINTERLEAVE_OUTPUT_TO_TEXTURE2DARRAY 0

RWTexture2D<float>                               g_t2dOutput                   : register(u0);
RWTexture2DArray<float>                          g_t2daOutput                  : register(u1);

#include "AMD_AOFX_Kernel.hlsl"

#if ( AOFX_IMPLEMENTATION == AOFX_IMPLEMENTATION_CS )

//==================================================================================================
// AO CS: Loads an overlapping tile of texels from the depth buffer (and optionally the normals buffer)
// It then converts depth into camera XYZ and optionally offsets by the camera space normal XYZ.
//==================================================================================================
[numthreads(AO_GROUP_THREAD_DIM, AO_GROUP_THREAD_DIM, 1)]
void csAmbientOcclusionDeinterleave(uint3 groupIdx    : SV_GroupID,
                                    uint3 threadIdx   : SV_GroupThreadID,
                                    uint  threadIndex : SV_GroupIndex,
                                    uint3 dispatchIdx : SV_DispatchThreadID)
{
  int2   screenCoord;
  float3 position;

  int2 layerIdx = MOD_AO_DEINTERLEAVE_FACTOR(groupIdx.xy);
  int  layerIndex = layerIdx.x + MUL_AO_DEINTERLEAVE_FACTOR(layerIdx.y);
  int2 layerGroupIdx = DIV_AO_DEINTERLEAVE_FACTOR(groupIdx.xy);

  int2 layerGroupCoord = layerGroupIdx.xy * uint2(AO_GROUP_THREAD_DIM, AO_GROUP_THREAD_DIM);
  int2 layerDispatchIdx = layerGroupCoord.xy + threadIdx.xy;

  screenCoord = layerDispatchIdx.xy + int2(-AO_GROUP_TEXEL_OVERLAP, -AO_GROUP_TEXEL_OVERLAP);
  position = loadCameraSpacePositionT2D(screenCoord, layerIdx);
  storePositionInCache(position, threadIdx.xy + int2(AO_GROUP_THREAD_DIM * 0, AO_GROUP_THREAD_DIM * 0));

  screenCoord = layerDispatchIdx.xy + int2(-AO_GROUP_TEXEL_OVERLAP, AO_GROUP_TEXEL_OVERLAP);
  position = loadCameraSpacePositionT2D(screenCoord, layerIdx);
  storePositionInCache(position, threadIdx.xy + int2(AO_GROUP_THREAD_DIM * 0, AO_GROUP_THREAD_DIM * 1));

  screenCoord = layerDispatchIdx.xy + int2(AO_GROUP_TEXEL_OVERLAP, -AO_GROUP_TEXEL_OVERLAP);
  position = loadCameraSpacePositionT2D(screenCoord, layerIdx);
  storePositionInCache(position, threadIdx.xy + int2(AO_GROUP_THREAD_DIM * 1, AO_GROUP_THREAD_DIM * 0));

  screenCoord = layerDispatchIdx.xy + int2(AO_GROUP_TEXEL_OVERLAP, AO_GROUP_TEXEL_OVERLAP);
  position = loadCameraSpacePositionT2D(screenCoord, layerIdx);
  storePositionInCache(position, threadIdx.xy + int2(AO_GROUP_THREAD_DIM * 1, AO_GROUP_THREAD_DIM * 1));

  GroupMemoryBarrierWithGroupSync();

  if ( (layerDispatchIdx.x < (int)g_cbAO.m_OutputSize.x) &&
       (layerDispatchIdx.y < (int)g_cbAO.m_OutputSize.y) &&
       (layerIndex < AO_DEINTERLEAVE_FACTOR_SQR) )
  {
    int2 cacheCoord = threadIdx.xy + int2(AO_GROUP_TEXEL_OVERLAP, AO_GROUP_TEXEL_OVERLAP);
    position = fetchPositionFromCache(layerDispatchIdx, cacheCoord, layerIdx);
    float ambientOcclusion = kernelHDAO(position, layerDispatchIdx.xy, cacheCoord, layerIndex, layerIdx, layerIndex);

# if AO_DEINTERLEAVE_OUTPUT_TO_TEXTURE2DARRAY
    g_t2daOutput[uint3(layerDispatchIdx.xy, layerIndex)] = ambientOcclusion;
# else
    g_t2dOutput[MUL_AO_DEINTERLEAVE_FACTOR(layerDispatchIdx) + layerIdx] = ambientOcclusion;
# endif
  }
}

#elif ( AOFX_IMPLEMENTATION == AOFX_IMPLEMENTATION_PS )

#include "../../../AMD_LIB/src/Shaders/AMD_FullscreenPass.hlsl"

void psAmbientOcclusionDeinterleave(PS_FullscreenInput In)
{
  uint2  deinterleavedSize = g_cbAO.m_OutputSize;
  uint2  dispatchIdx = (uint2) (In.position.xy);
  int2   layerIdx = dispatchIdx / deinterleavedSize;
  int    layerIndex = layerIdx.x + layerIdx.y * AO_DEINTERLEAVE_FACTOR;
  uint2  layerDispatchIdx = dispatchIdx % deinterleavedSize;
  float3 position = fetchPositionFromCache(layerDispatchIdx.xy, uint2(0, 0), layerIdx);
  float  ambientOcclusion = kernelHDAO(position, layerDispatchIdx.xy, int2(0, 0), layerIndex, layerIdx, layerIndex);

    if ( (layerDispatchIdx.x < g_cbAO.m_OutputSize.x) &&
         (layerDispatchIdx.y < g_cbAO.m_OutputSize.y) &&
         (layerIndex < AO_DEINTERLEAVE_FACTOR_SQR) )
      g_t2dOutput[MUL_AO_DEINTERLEAVE_FACTOR(layerDispatchIdx) + layerIdx] = ambientOcclusion;
}

#endif // ( AOFX_IMPLEMENTATION == AOFX_IMPLEMENTATION_CS )

//======================================================================================================
// EOF
//======================================================================================================