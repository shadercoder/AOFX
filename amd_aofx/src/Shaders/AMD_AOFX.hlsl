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

//==================================================================================================
// AO UAVs, Textures & Samplers
//==================================================================================================
RWTexture2D<float>                               g_t2dOutput                   : register( u0 );

#include "AMD_AOFX_Kernel.hlsl"

#if ( AOFX_IMPLEMENTATION == AOFX_IMPLEMENTATION_CS )

//==================================================================================================
// AO CS: Loads an overlapping tile of texels from the pre-processed depth buffer 
// It then converts depth into camera XYZ 
// If normals are used, the convertion from depth -> camera XYZ happens at a preprocess step
// and camera XYZ is additionally displaced by the normal vector 
//==================================================================================================
[numthreads( AO_GROUP_THREAD_DIM, AO_GROUP_THREAD_DIM, 1 )]
void csAmbientOcclusion(uint3 groupIdx    : SV_GroupID,
                        uint3 threadIdx   : SV_GroupThreadID,
                        uint  threadIndex : SV_GroupIndex,
                        uint3 dispatchIdx : SV_DispatchThreadID )
{
  float2 screenCoord;
  float3 position;

  float2 baseCoord = dispatchIdx.xy + float2(0.5f, 0.5f);

  screenCoord = baseCoord + float2(-AO_GROUP_TEXEL_OVERLAP, -AO_GROUP_TEXEL_OVERLAP);
  position = loadCameraSpacePositionT2D(screenCoord, uint2(0, 0));
  storePositionInCache(position, threadIdx.xy + int2(AO_GROUP_THREAD_DIM * 0, AO_GROUP_THREAD_DIM * 0));

  screenCoord = baseCoord + float2(-AO_GROUP_TEXEL_OVERLAP, AO_GROUP_TEXEL_OVERLAP);
  position = loadCameraSpacePositionT2D(screenCoord, uint2(0, 0));
  storePositionInCache(position, threadIdx.xy + int2(AO_GROUP_THREAD_DIM * 0, AO_GROUP_THREAD_DIM * 1));

  screenCoord = baseCoord + float2(AO_GROUP_TEXEL_OVERLAP, -AO_GROUP_TEXEL_OVERLAP);
  position = loadCameraSpacePositionT2D(screenCoord, uint2(0, 0));
  storePositionInCache(position, threadIdx.xy + int2(AO_GROUP_THREAD_DIM * 1, AO_GROUP_THREAD_DIM * 0));

  screenCoord = baseCoord + float2(AO_GROUP_TEXEL_OVERLAP, AO_GROUP_TEXEL_OVERLAP);
  position = loadCameraSpacePositionT2D(screenCoord, uint2(0, 0));
  storePositionInCache(position, threadIdx.xy + int2(AO_GROUP_THREAD_DIM * 1, AO_GROUP_THREAD_DIM * 1));

  GroupMemoryBarrierWithGroupSync();

  if ( (dispatchIdx.x < g_cbAO.m_OutputSize.x) && (dispatchIdx.y < g_cbAO.m_OutputSize.y) )
  {
    uint2 cacheCoord = threadIdx.xy + int2(AO_GROUP_TEXEL_OVERLAP, AO_GROUP_TEXEL_OVERLAP);
    screenCoord = baseCoord;
    position = fetchPositionFromCache(screenCoord.xy, cacheCoord, int2(0, 0));
    uint randomIndex = (dispatchIdx.x * dispatchIdx.y) % AO_RANDOM_TAPS_COUNT;

    float ambientOcclusion = kernelHDAO(position, screenCoord.xy, cacheCoord, randomIndex, uint2(0, 0), 0);

    g_t2dOutput[dispatchIdx.xy] = ambientOcclusion;
  }
}

#elif ( AOFX_IMPLEMENTATION == AOFX_IMPLEMENTATION_PS )

#include "../../../AMD_LIB/src/Shaders/AMD_FullscreenPass.hlsl"

void psAmbientOcclusion ( PS_FullscreenInput In )
{
  uint2  dispatchIdx = uint2(In.position.xy);
  float2 screenCoord = In.position.xy;
  float3 position = fetchPositionFromCache(screenCoord, uint2(0, 0), int2(0, 0));
  uint randomIndex = (dispatchIdx.x * dispatchIdx.y) % AO_RANDOM_TAPS_COUNT;

  float ambientOcclusion = kernelHDAO(position, screenCoord.xy, uint2(0, 0), randomIndex, uint2(0, 0), 0);

  if ( (dispatchIdx.x < g_cbAO.m_OutputSize.x) && 
       (dispatchIdx.y < g_cbAO.m_OutputSize.y) )
    g_t2dOutput[dispatchIdx] = ambientOcclusion;
}

#endif // AOFX_IMPLEMENTATION

//======================================================================================================
// EOF
//======================================================================================================