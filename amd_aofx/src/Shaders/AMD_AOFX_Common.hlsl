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

#ifndef _AMD_AOFX_COMMON_HLSL_
#define _AMD_AOFX_COMMON_HLSL_

//======================================================================================================
// AO parameter constants
//======================================================================================================
#define AO_GROUP_THREAD_DIM                      ( 32 )                      // 32 * 32 = 1024 threads
#define AO_GROUP_TEXEL_DIM                       ( AO_GROUP_THREAD_DIM * 2 ) // 32 * 2 = 64 ; 64 x 64 is the size of loaded tile
#define AO_GROUP_TEXEL_OVERLAP                   ( AO_GROUP_THREAD_DIM / 2 ) // overlap is 16 texels

#define AO_ULTRA_SAMPLE_COUNT                    32
#define AO_HIGH_SAMPLE_COUNT                     24
#define AO_MEDIUM_SAMPLE_COUNT                   16
#define AO_LOW_SAMPLE_COUNT                      8

#define AOFX_NORMAL_OPTION_NONE                  0
#define AOFX_NORMAL_OPTION_READ_FROM_SRV         1

#define AOFX_TAP_TYPE_FIXED                      0
#define AOFX_TAP_TYPE_RANDOM_CB                  1
#define AOFX_TAP_TYPE_RANDOM_SRV                 2

#define AOFX_IMPLEMENTATION_CS                   0
#define AOFX_IMPLEMENTATION_PS                   1

#define AO_STORE_XY_LDS                          1

#define AO_RANDOM_TAPS_COUNT                     64 

#ifndef AOFX_IMPLEMENTATION
#   define AOFX_IMPLEMENTATION                   AOFX_IMPLEMENTATION_CS
#endif

#ifndef AOFX_NORMAL_OPTION
#   define AOFX_NORMAL_OPTION                    AOFX_NORMAL_OPTION_NONE
#endif

#ifndef AOFX_TAP_TYPE
#   define AOFX_TAP_TYPE                         AOFX_TAP_TYPE_FIXED
#endif

#ifndef AOFX_MSAA_LEVEL
#   define AOFX_MSAA_LEVEL                       1
#endif

#if (AOFX_NORMAL_OPTION == AOFX_NORMAL_OPTION_NONE)
# define AO_INPUT_TYPE                           float
#elif (AOFX_NORMAL_OPTION == AOFX_NORMAL_OPTION_READ_FROM_SRV)
# define AO_INPUT_TYPE                           float4
#endif

#ifndef AO_DEINTERLEAVE_FACTOR 
# define AO_DEINTERLEAVE_FACTOR                  4
#endif

#define AO_DEINTERLEAVE_FACTOR_SQR               (AO_DEINTERLEAVE_FACTOR*AO_DEINTERLEAVE_FACTOR)

#define MUL_AO_DEINTERLEAVE_FACTOR(coord)       ((coord) * AO_DEINTERLEAVE_FACTOR)    
#define DIV_AO_DEINTERLEAVE_FACTOR(coord)       ((coord) / AO_DEINTERLEAVE_FACTOR)  
#define MOD_AO_DEINTERLEAVE_FACTOR(coord)       ((coord) % AO_DEINTERLEAVE_FACTOR) 

//======================================================================================================
// AO structures
//======================================================================================================

struct AO_Data
{
  uint2                                          m_OutputSize;
  float2                                         m_OutputSizeRcp;
  uint2                                          m_InputSize;                 // size (xy), inv size (zw)
  float2                                         m_InputSizeRcp;                  // size (xy), inv size (zw)

  float                                          m_CameraQ;                    // far / (far - near)
  float                                          m_CameraQTimesZNear;          // cameraQ * near
  float                                          m_CameraTanHalfFovHorizontal; // Tan Horiz and Vert FOV
  float                                          m_CameraTanHalfFovVertical;

  float                                          m_RejectRadius;
  float                                          m_AcceptRadius;
  float                                          m_RecipFadeOutDist;
  float                                          m_LinearIntensity;

  float                                          m_NormalScale;
  float                                          m_MultiResLayerScale;
  float                                          m_ViewDistanceFade;
  float                                          m_ViewDistanceDiscard;

  float                                          m_FadeIntervalLength;
  float3                                         _pad;
};

struct AO_InputData
{
  uint2                                          m_OutputSize;
  float2                                         m_OutputSizeRcp;
  uint2                                          m_InputSize;                 // size (xy), inv size (zw)
  float2                                         m_InputSizeRcp;              // size (xy), inv size (zw)

  float                                          m_ZFar;
  float                                          m_ZNear;
  float                                          m_CameraQ;                    // far / (far - near)
  float                                          m_CameraQTimesZNear;          // cameraQ * near
  float                                          m_CameraTanHalfFovHorizontal; // Tan Horiz and Vert FOV
  float                                          m_CameraTanHalfFovVertical;

  float                                          m_DepthUpsampleThreshold;
  float                                          m_NormalScale;

  float                                          m_Scale;
  float                                          m_ScaleRcp;
  float                                          m_ViewDistanceFade;
  float                                          m_ViewDistanceDiscard;
};

//======================================================================================================
// AO constant buffers, samplers and textures
//======================================================================================================

SamplerState                                     g_ssPointClamp                : register( s0 );
SamplerState                                     g_ssLinearClamp               : register( s1 );
SamplerState                                     g_ssPointWrap                 : register( s2 );
SamplerState                                     g_ssLinearWrap                : register( s3 );

#if (AOFX_MSAA_LEVEL == 1)
Texture2D< float >                               g_t2dDepth                    : register( t0 );
Texture2D< float4 >                              g_t2dNormal                   : register( t1 );
#else
Texture2DMS< float, AOFX_MSAA_LEVEL >              g_t2dDepth                    : register( t0 );
Texture2DMS< float4, AOFX_MSAA_LEVEL >             g_t2dNormal                   : register( t1 );
#endif

Buffer<int2>                                     g_b1dSamplePattern            : register( t2 );

#if (AO_DEINTERLEAVE_FACTOR == 1)
Texture2D<AO_INPUT_TYPE>                         g_t2dInput                    : register( t3 );
#else
Texture2DArray<AO_INPUT_TYPE>                    g_t2daInput                   : register( t3 );
#endif

cbuffer                                          CB_AO_DATA                    : register( b0 )
{
  AO_Data                                        g_cbAO;
};

cbuffer                                          CB_SAMPLE_PATTERN             : register( b1 )
{
  int4                                           g_cbSamplePattern[64][32];
};

#if defined( ULTRA_SAMPLES )

# define NUM_VALLEYS                             AO_ULTRA_SAMPLE_COUNT
static const int2                                g_SamplePattern[NUM_VALLEYS] =
{
  {0, -9}, {4, -9}, {2, -6}, {6, -6},
  {0, -3}, {4, -3}, {8, -3}, {2, 0},
  {6, 0}, {9, 0}, {4, 3}, {8, 3},
  {2, 6}, {6, 6}, {9, 6}, {4, 9},
  {10, 0}, {-12, 12}, {9, -14}, {-8, -6},
  {11, -7}, {-9, 1}, {-2, -13}, {-7, -3},
  {4, 7}, {3, -13}, {12, 3}, {-12, 8},
  {-10, 13}, {12, 1}, {9, 13}, {0, -5},
};

#elif defined( HIGH_SAMPLES )

# define NUM_VALLEYS                             AO_HIGH_SAMPLE_COUNT
static const int2                                g_SamplePattern[NUM_VALLEYS] =
{
  {0, -9}, {4, -9}, {2, -6}, {6, -6},
  {0, -3}, {4, -3}, {8, -3}, {2, 0},
  {6, 0}, {9, 0}, {4, 3}, {8, 3},
  {2, 6}, {6, 6}, {9, 6}, {4, 9},
  {10, 0}, {-12, 12}, {9, -14}, {-8, -6},
  {11, -7}, {-9, 1}, {-2, -13}, {-7, -3},
};

#elif defined( MEDIUM_SAMPLES )

# define NUM_VALLEYS                             AO_MEDIUM_SAMPLE_COUNT
static const int2                                g_SamplePattern[NUM_VALLEYS] =
{
  {0, -9}, {4, -9}, {2, -6}, {6, -6},
  {0, -3}, {4, -3}, {8, -3}, {2, 0},
  {6, 0}, {9, 0}, {4, 3}, {8, 3},
  {2, 6}, {6, 6}, {9, 6}, {4, 9},
};

#else //if defined( LOW_SAMPLES )

# define NUM_VALLEYS                             AO_LOW_SAMPLE_COUNT
static const int2                                g_SamplePattern[NUM_VALLEYS] =
{
  {0, -9}, {2, -6}, {0, -3}, {8, -3},
  {6, 0}, {4, 3}, {2, 6}, {9, 6},
};

#endif // ULTRA_SAMPLES

//======================================================================================================
// AO packing function
//======================================================================================================

uint normalToUint16(float3 unitNormal, out int zSign)
{
  zSign = sign(unitNormal.z);

  int x = (unitNormal.x * 0.5 + 0.5) * 255.0f;
  int y = (unitNormal.y * 0.5 + 0.5) * 255.0f;

  int result = (x & 0x000000FF) | ((y & 0x000000FF) << 8);

  return result;
}

float3 uint16ToNormal(uint normal, int zSign)
{
  float3 result;

  int x = (normal & 0x000000FF);
  int y = (normal & 0x0000FF00) >> 8;

  result.x = ((x / 255.0f) - 0.5f) * 2.0f;
  result.y = ((y / 255.0f) - 0.5f) * 2.0f;

  result.z = zSign * sqrt(1 - result.x * result.x - result.y * result.y);

  return result;
}

uint depthToUint16(float linearDepth)
{
  return f32tof16(linearDepth);
}

float uint16ToDepth(uint depth)
{
  return f16tof32(depth);
}

// Packs a float2 to a unit
uint float2ToUint32(float2 f2Value)
{
  uint uRet = 0;

  uRet = (f32tof16(f2Value.x)) + (f32tof16(f2Value.y) << 16);

  return uRet;
}

uint int2ToUint(int2 i2Value)
{
  uint uRet = 0;

  uRet = (i2Value.x) + (i2Value.y << 16);

  return uRet;
}

// Unpacks a uint to a float2
float2 uintToFloat2(uint uValue)
{
  return float2(f16tof32(uValue), f16tof32(uValue >> 16));
}

int2 uintToInt2(uint uValue)
{
  return int2(uValue & 65536, uValue >> 16);
}

float3 loadCameraSpacePositionT2D( float2 screenCoord, int2 layerIdx )
{

#if (AO_DEINTERLEAVE_FACTOR == 1)
  AO_INPUT_TYPE aoInput = g_t2dInput.SampleLevel(g_ssPointClamp, screenCoord.xy * g_cbAO.m_OutputSizeRcp, 0.0f);
#else //  (AO_DEINTERLEAVE_FACTOR == 1)
  int layer = layerIdx.x + MUL_AO_DEINTERLEAVE_FACTOR(layerIdx.y);
  AO_INPUT_TYPE aoInput = g_t2daInput.SampleLevel(g_ssPointClamp, float3((screenCoord.xy + float2(0.5, 0.5)) * g_cbAO.m_OutputSizeRcp, layer), 0.0f);
#endif //  (AO_DEINTERLEAVE_FACTOR == 1)

#if (AOFX_NORMAL_OPTION == AOFX_NORMAL_OPTION_NONE)

  float camera_z = aoInput.x;

# if (AO_DEINTERLEAVE_FACTOR == 1)
  float2 camera = screenCoord.xy * g_cbAO.m_InputSizeRcp - float2(1.0f, 1.0f);
# else //  (AO_DEINTERLEAVE_FACTOR == 1)
  float2 camera = (MUL_AO_DEINTERLEAVE_FACTOR(screenCoord.xy) + layerIdx + float2(0.5, 0.5)) * g_cbAO.m_InputSizeRcp - float2(1.0f, 1.0f);
# endif //  (AO_DEINTERLEAVE_FACTOR == 1)

  camera.x = camera.x * camera_z * g_cbAO.m_CameraTanHalfFovHorizontal;
  camera.y = camera.y * camera_z * -g_cbAO.m_CameraTanHalfFovVertical;

  float3 position = float3(camera.xy, camera_z);

#elif (AOFX_NORMAL_OPTION == AOFX_NORMAL_OPTION_READ_FROM_SRV)

  float3 position = aoInput.yzw;

#endif

  return position;
}

float loadCameraSpaceDepthT2D( float2 uv, float cameraQ, float cameraZNearXQ )
{

  float depth = g_t2dDepth.SampleLevel(g_ssPointClamp, uv, 0.0f);

  float camera_z = -cameraZNearXQ / ( depth - cameraQ );

  return camera_z;
}

#endif // _AMD_AO_COMMON_HLSL_