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

#define DEINTERLEAVE_IMPLEMENTATION_CS           0
#define DEINTERLEAVE_IMPLEMENTATION_PS           1

#ifndef DEINTERLEAVE_IMPLEMENTATION
# define DEINTERLEAVE_IMPLEMENTATION             DEINTERLEAVE_IMPLEMENTATION_CS
#endif

#if (AOFX_NORMAL_OPTION == AOFX_NORMAL_OPTION_NONE)
# define DEINTERLEAVE_TYPE                       float
#elif (AOFX_NORMAL_OPTION == AOFX_NORMAL_OPTION_READ_FROM_SRV)
# define DEINTERLEAVE_TYPE                       float4
#endif

#ifndef DEINTERLEAVE_FACTOR 
# define DEINTERLEAVE_FACTOR                     4 
#endif

#define DEINTERLEAVE_FACTOR_SQR                 (DEINTERLEAVE_FACTOR*DEINTERLEAVE_FACTOR)

#define DEINTERLEAVE_USING_SHARED_MEMORY         1

#define GROUP_THREAD_DIM_X                       32  
#define GROUP_THREAD_DIM_Y                       32  
#define GROUP_THREAD_DIM_Z                       1

#define DEINTERLEAVE_GROUP_DIM_X                 ( GROUP_THREAD_DIM_X / DEINTERLEAVE_FACTOR)
#define DEINTERLEAVE_GROUP_DIM_Y                 ( GROUP_THREAD_DIM_Y / DEINTERLEAVE_FACTOR)
#define DEINTERLEAVE_GROUP_DIM                   DEINTERLEAVE_GROUP_DIM_X
#define DEINTERLEAVE_GROUP_DIM_SQR               ( DEINTERLEAVE_GROUP_DIM * DEINTERLEAVE_GROUP_DIM )

#define DEINTERLEAVE_GROUP_SHARED_DIM_X          GROUP_THREAD_DIM_X  
#define DEINTERLEAVE_GROUP_SHARED_DIM_Y          GROUP_THREAD_DIM_Y  

# define MUL_DEINTERLEAVE_FACTOR(coord)          ((coord) * DEINTERLEAVE_FACTOR)    
# define DIV_DEINTERLEAVE_FACTOR(coord)          ((coord) / DEINTERLEAVE_FACTOR) 
# define MOD_DEINTERLEAVE_FACTOR(coord)          ((coord) % DEINTERLEAVE_FACTOR) 

# define MUL_DEINTERLEAVE_GROUP_DIM(coord)       ((coord) * DEINTERLEAVE_GROUP_DIM)    
# define DIV_DEINTERLEAVE_GROUP_DIM(coord)       ((coord) / DEINTERLEAVE_GROUP_DIM)   
# define MOD_DEINTERLEAVE_GROUP_DIM(coord)       ((coord) % DEINTERLEAVE_GROUP_DIM)   
# define DIV_DEINTERLEAVE_GROUP_DIM_SQR(coord)   ((coord) / DEINTERLEAVE_GROUP_DIM_SQR)
# define MOD_DEINTERLEAVE_GROUP_DIM_SQR(coord)   ((coord) % DEINTERLEAVE_GROUP_DIM_SQR)

cbuffer                                          CB_DEINTERLEAVE_DATA          : register( b0 )
{
  AO_InputData                                   g_cbInputData;
};

#if ( DEINTERLEAVE_IMPLEMENTATION == DEINTERLEAVE_IMPLEMENTATION_CS )

#if (DEINTERLEAVE_FACTOR == 1)
RWTexture2D<DEINTERLEAVE_TYPE>                   g_t2dDeinterleavedInput      : register(u0);
#else
RWTexture2DArray<DEINTERLEAVE_TYPE>              g_t2daDeinterleavedInput      : register(u0);
#endif

groupshared DEINTERLEAVE_TYPE                    g_SharedInput[DEINTERLEAVE_GROUP_SHARED_DIM_X][DEINTERLEAVE_GROUP_SHARED_DIM_Y+1];

DEINTERLEAVE_TYPE LoadInput(float2 uv, float znear, float zfar)
{
  DEINTERLEAVE_TYPE result = (DEINTERLEAVE_TYPE)0.0f;

  float q = g_cbInputData.m_CameraQ;
  float znear_x_q = g_cbInputData.m_CameraQTimesZNear; 

  float depth = g_t2dDepth.SampleLevel(g_ssPointClamp, uv, 0).x;

  float camera_z = -znear_x_q / ( depth - q );

# if (AOFX_NORMAL_OPTION == AOFX_NORMAL_OPTION_READ_FROM_SRV)
  float3 normal = g_t2dNormal.SampleLevel(g_ssPointClamp, uv, 0).xyz - float3(0.5, 0.5, 0.5);

  float2 camera = uv * float2(2.0f, 2.0f) - float2( 1.0f, 1.0f );
  camera.x = camera.x * camera_z * g_cbInputData.m_CameraTanHalfFovHorizontal;
  camera.y = camera.y * camera_z * -g_cbInputData.m_CameraTanHalfFovVertical;

  result = float4(camera_z, float3(camera, camera_z) + normal * g_cbInputData.m_NormalScale);
# else
  result = camera_z;
# endif

  return result;
}

//==================================================================================================
// Deinterleave CS: Loads a tile of texels from the depth and normal t2d 
// It then converts depth into camera Z and deinterleaves into GROUP_THREAD_DIM_Z layers
// If normals are available the depth is converted to camera XYZ and displaced along the normal
//==================================================================================================
[numthreads( GROUP_THREAD_DIM_X, GROUP_THREAD_DIM_Y, GROUP_THREAD_DIM_Z )]
void csDeinterleave(uint3 groupIdx    : SV_GroupID,
                    uint3 threadIdx   : SV_GroupThreadID,
                    uint  threadIndex : SV_GroupIndex,
                    uint3 dispatchIdx : SV_DispatchThreadID )
{
  // Deinterleaving through shared memory seems to be faster than doing so directly 
# if (DEINTERLEAVE_USING_SHARED_MEMORY==1)
  float2 uv = (dispatchIdx.xy + float2(0.5f, 0.5f)) * g_cbInputData.m_InputSizeRcp;

  DEINTERLEAVE_TYPE input = LoadInput(uv, g_cbInputData.m_ZNear, g_cbInputData.m_ZFar);

  g_SharedInput[threadIdx.y][threadIdx.x] = input;
  GroupMemoryBarrierWithGroupSync();

  uint2 layerIdx = DIV_DEINTERLEAVE_GROUP_DIM( threadIdx.xy ); // == (0..7, 0..7)
  uint2 ldsAddress = MUL_DEINTERLEAVE_FACTOR( MOD_DEINTERLEAVE_GROUP_DIM( threadIdx.xy ) ) + layerIdx;

  uint2 deinterleaveDispatchIdx = MUL_DEINTERLEAVE_GROUP_DIM( groupIdx.xy ) + MOD_DEINTERLEAVE_GROUP_DIM( threadIdx.xy );

  uint layerIndex = layerIdx.x + MUL_DEINTERLEAVE_FACTOR(layerIdx.y);

  if (deinterleaveDispatchIdx.x < g_cbInputData.m_OutputSize.x &&
      deinterleaveDispatchIdx.y < g_cbInputData.m_OutputSize.y )
#if (DEINTERLEAVE_FACTOR == 1)
      g_t2dDeinterleavedInput[uint2(deinterleaveDispatchIdx.xy)] = g_SharedInput[ldsAddress.y][ldsAddress.x];
#else
      g_t2daDeinterleavedInput[uint3(deinterleaveDispatchIdx.xy, layerIndex)] = g_SharedInput[ldsAddress.y][ldsAddress.x];
#endif

# elif (DEINTERLEAVE_USING_SHARED_MEMORY==2) 

  float2 uv = (dispatchIdx.xy + float2(0.5f, 0.5f)) * g_cbInputData.m_InputSizeRcp;

  DEINTERLEAVE_TYPE input = LoadInput(uv, g_cbInputData.m_ZNear, g_cbInputData.m_ZFar);

  g_SharedInput[threadIdx.y][threadIdx.x] = input;
  GroupMemoryBarrierWithGroupSync();

  uint layerIndex = DIV_DEINTERLEAVE_GROUP_DIM_SQR(threadIndex); 
  uint2 layerIdx = uint2( MOD_DEINTERLEAVE_FACTOR(layerIndex), DIV_DEINTERLEAVE_FACTOR(layerIndex) );

  uint layerThreadIndex = MOD_DEINTERLEAVE_GROUP_DIM_SQR(threadIndex); 
  uint2 layerThreadIdx = uint2( MOD_DEINTERLEAVE_GROUP_DIM(layerThreadIndex), DIV_DEINTERLEAVE_GROUP_DIM(layerThreadIndex) );

  uint2 ldsAddress = MUL_DEINTERLEAVE_FACTOR(layerThreadIdx) + layerIdx;

  uint2 deinterleaveDispatchIdx = MUL_DEINTERLEAVE_GROUP_DIM( groupIdx.xy ) + layerThreadIdx;

  if (deinterleaveDispatchIdx.x < g_cbInputData.m_OutputSize.x &&
      deinterleaveDispatchIdx.y < g_cbInputData.m_OutputSize.y )
#if (DEINTERLEAVE_FACTOR == 1)
      g_t2dDeinterleavedInput[uint2(deinterleaveDispatchIdx.xy)] = g_SharedInput[ldsAddress.y][ldsAddress.x];
#else
      g_t2daDeinterleavedInput[uint3(deinterleaveDispatchIdx.xy, layerIndex)] = g_SharedInput[ldsAddress.y][ldsAddress.x];
#endif

# else

  uint2 layerIdx = DIV_DEINTERLEAVE_GROUP_DIM( threadIdx.xy ); // == (0..7, 0..7)
  uint2 ldsAddress = MUL_DEINTERLEAVE_FACTOR( MOD_DEINTERLEAVE_GROUP_DIM( threadIdx.xy ) ) + layerIdx;

  float2 uv = (groupIdx.xy * uint2(GROUP_THREAD_DIM_X, GROUP_THREAD_DIM_Y) + ldsAddress + float2(0.5f, 0.5f)) * g_cbInputData.m_InputSizeRcp;

  DEINTERLEAVE_TYPE input = LoadInput(uv, g_cbInputData.m_ZNear, g_cbInputData.m_ZFar);

  uint2 deinterleaveDispatchIdx = MUL_DEINTERLEAVE_GROUP_DIM( groupIdx.xy )+ MOD_DEINTERLEAVE_GROUP_DIM( threadIdx.xy );

  uint layerIndex = layerIdx.x + MUL_DEINTERLEAVE_FACTOR(layerIdx.y);

  if (deinterleaveDispatchIdx.x < g_cbInputData.m_OutputSize.x &&
      deinterleaveDispatchIdx.y < g_cbInputData.m_OutputSize.y )
#if (DEINTERLEAVE_FACTOR == 1)
      g_t2dDeinterleavedInput[uint2(deinterleaveDispatchIdx.xy)] = input;
#else
      g_t2daDeinterleavedInput[uint3(deinterleaveDispatchIdx.xy, layerIndex)] = input;
#endif
    
# endif
}

Texture2DArray<float>                            g_t2daAmbientOcclusion        : register( t0 );
RWTexture2D<float>                               g_t2dAmbientOcclusion         : register( u0 );
groupshared float                                g_SharedAmbientOcclusion[DEINTERLEAVE_GROUP_SHARED_DIM_X][DEINTERLEAVE_GROUP_SHARED_DIM_Y];

float LoadArrayInput(float2 uv, uint layer)
{
  return g_t2daAmbientOcclusion.SampleLevel(g_ssPointClamp, float3(uv, layer), 0.0f).x;
}

//==================================================================================================
// Interleave CS (not used): Loads a tile of texels from the ambient occlusion t2da 
// It then interleaves into a full resolution ambient occlusion t2d
//==================================================================================================
[numthreads( GROUP_THREAD_DIM_X, GROUP_THREAD_DIM_Y, GROUP_THREAD_DIM_Z )]
void csInterleave(uint3 groupIdx    : SV_GroupID,
                  uint3 threadIdx   : SV_GroupThreadID,
                  uint  threadIndex : SV_GroupIndex,
                  uint3 dispatchIdx : SV_DispatchThreadID )
{
  uint2 layerIdx = DIV_DEINTERLEAVE_GROUP_DIM( threadIdx.xy );
  uint2 deinterleaveDispatchIdx = MUL_DEINTERLEAVE_GROUP_DIM( groupIdx.xy ) + MOD_DEINTERLEAVE_GROUP_DIM( threadIdx.xy );
  uint layerIndex = layerIdx.x + MUL_DEINTERLEAVE_FACTOR(layerIdx.y);

  float2 uv = (deinterleaveDispatchIdx + float2(0.5f, 0.5f)) * g_cbInputData.m_OutputSizeRcp;
  float input = LoadArrayInput(uv, layerIndex);

  uint2 ldsAddress = MUL_DEINTERLEAVE_FACTOR( MOD_DEINTERLEAVE_GROUP_DIM( threadIdx.xy ) ) + layerIdx;

  g_SharedAmbientOcclusion[ldsAddress.y][ldsAddress.x] = input;

  GroupMemoryBarrierWithGroupSync();

  if (dispatchIdx.x < (uint)g_cbInputData.m_InputSize.x &&
      dispatchIdx.y < (uint)g_cbInputData.m_InputSize.y )
  {
    int x_mod_2 = groupIdx.x % 2;
    int y_mod_2 = groupIdx.y % 2;
    float shade = 1.00f;
    if (x_mod_2 != y_mod_2) shade = 1.0;
    g_t2dAmbientOcclusion[dispatchIdx.xy] = g_SharedAmbientOcclusion[threadIdx.y][threadIdx.x] * shade;
  }
}

#else // if ( DEINTERLEAVE_IMPLEMENTATION == DEINTERLEAVE_IMPLEMENTATION_CS )

#include "../../../AMD_LIB/src/Shaders/AMD_FullscreenPass.hlsl"

#if (DEINTERLEAVE_FACTOR == 1)
RWTexture2D<DEINTERLEAVE_TYPE>                   g_t2dDeinterleavedInput      : register(u0);
#else
RWTexture2DArray<DEINTERLEAVE_TYPE>              g_t2daDeinterleavedInput      : register(u0);
#endif

void psDeinterleave( PS_FullscreenInput In )
{
  DEINTERLEAVE_TYPE O;

  uint2 dispatchIdx = (uint2) (In.position.xy);
  uint2 layerIdx = dispatchIdx % DEINTERLEAVE_FACTOR;
  uint  layerIndex = layerIdx.x + layerIdx.y * DEINTERLEAVE_FACTOR;
  uint2 layerDispatchIdx = dispatchIdx / DEINTERLEAVE_FACTOR;

  float2 coord = ( floor((layerDispatchIdx * DEINTERLEAVE_FACTOR + layerIdx) * g_cbInputData.m_ScaleRcp) + float2(0.5f, 0.5f));
  float2 texCoord = coord * g_cbInputData.m_InputSizeRcp;

  float depth    = g_t2dDepth.SampleLevel( g_ssPointClamp, texCoord, 0 ).x;
  float camera_z = -g_cbInputData.m_CameraQTimesZNear / ( depth - g_cbInputData.m_CameraQ );

# if (AOFX_NORMAL_OPTION == AOFX_NORMAL_OPTION_READ_FROM_SRV)

  float3 normal = g_t2dNormal.SampleLevel( g_ssPointClamp, texCoord, 0 ).xyz - float3(0.5f, 0.5f, 0.5f);

  float2 camera = texCoord * float2(2.0f, 2.0f) - float2( 1.0f, 1.0f );
  camera.x = camera.x * camera_z * g_cbInputData.m_CameraTanHalfFovHorizontal;
  camera.y = camera.y * camera_z * -g_cbInputData.m_CameraTanHalfFovVertical;

  float3 position = float3(camera.xy, camera_z) + normal * g_cbInputData.m_NormalScale;

  O = float4(camera_z, position);

# elif (AOFX_NORMAL_OPTION == AOFX_NORMAL_OPTION_NONE)

  O = camera_z;

# endif

#if (DEINTERLEAVE_FACTOR == 1)
  g_t2dDeinterleavedInput[uint2(layerDispatchIdx.xy)] = O; //
#else
  g_t2daDeinterleavedInput[uint3(layerDispatchIdx.xy, layerIndex)] = O; //
#endif
}

#endif // if ( DEINTERLEAVE_IMPLEMENTATION == DEINTERLEAVE_IMPLEMENTATION_CS )