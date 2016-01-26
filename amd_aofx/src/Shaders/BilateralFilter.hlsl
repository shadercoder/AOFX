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


#include "AOFX_SeparableFilter\\FilterCommon.hlsl"

#define PI                      ( 3.1415927f )
#define GAUSSIAN_DEVIATION      ( KERNEL_RADIUS * 0.5f )

// The input textures
Texture2D g_txDepth             : register( t0 );
Texture2D g_txAO                : register( t2 );

struct AO_InputData
{
  uint2                         m_OutputSize;
  float2                        m_OutputSizeRcp;
  uint2                         m_InputSize;                 // size (xy), inv size (zw)
  float2                        m_InputSizeRcp;              // size (xy), inv size (zw)

  float                         m_ZFar;
  float                         m_ZNear;
  float                         m_CameraQ;                    // far / (far - near)
  float                         m_CameraQTimesZNear;          // cameraQ * near
  float                         m_CameraTanHalfFovHorizontal; // Tan Horiz and Vert FOV
  float                         m_CameraTanHalfFovVertical;

  float                         m_DepthUpsampleThreshold;
  float                         m_NormalScale;

  float                         m_Scale;
  float                         m_ScaleRcp;
  float                         m_ViewDistanceFade;
  float                         m_ViewDistanceDiscard;
};

// Constant buffer used by both the CS & PS
cbuffer cbBilateralDilate       : register( b0 )
{
  AO_InputData                  g_aoInputData;
};


//--------------------------------------------------------------------------------------
// Hard coded AO params
//--------------------------------------------------------------------------------------
static float g_fAORejectRadius        = 0.8f;      // Camera Z values must fall within the reject and accept radius to be
static float g_fDepthFallOff          = g_fAORejectRadius / 7.0f; // Used by the bilateral filter to stop bleeding over steps in depth


// The output UAV used by the CS
RWTexture2D<float4> g_uavOutput : register( u0 );

// CS output structure
struct CS_Output
{
    float fColor[PIXELS_PER_THREAD];
};

// PS output structure
struct PS_Output
{
    float fColor[1];
};

// Uncompressed data as sampled from inputs
struct RAWDataItem
{
    float fAO;
    float fDepth;
};

// Data stored for the kernel
struct KernelData
{
    float fWeight;
    float fWeightSum;
    float fCenterAO;
    float fCenterDepth;
};


//--------------------------------------------------------------------------------------
// LDS definition and access macros
//--------------------------------------------------------------------------------------
#if ( USE_COMPUTE_SHADER == 1 )

#if( LDS_PRECISION == 32 )

struct LDS_Layout
{
    float  fAO;
    float  fDepth;
};

groupshared struct
{
    LDS_Layout Item[RUN_LINES][RUN_SIZE_PLUS_KERNEL];
}g_LDS;

#define WRITE_TO_LDS( _RAWDataItem, _iLineOffset, _iPixelOffset ) \
    g_LDS.Item[_iLineOffset][_iPixelOffset].fAO = _RAWDataItem.fAO; \
    g_LDS.Item[_iLineOffset][_iPixelOffset].fDepth = _RAWDataItem.fDepth;

#define READ_FROM_LDS( _iLineOffset, _iPixelOffset, _RAWDataItem ) \
    _RAWDataItem.fAO = g_LDS.Item[_iLineOffset][_iPixelOffset].fAO; \
    _RAWDataItem.fDepth = g_LDS.Item[_iLineOffset][_iPixelOffset].fDepth;

#elif( LDS_PRECISION == 16 )

struct LDS_Layout
{
    uint   uBoth;
};

groupshared struct
{
    LDS_Layout Item[RUN_LINES][RUN_SIZE_PLUS_KERNEL];
}g_LDS;

#define WRITE_TO_LDS( _RAWDataItem, _iLineOffset, _iPixelOffset ) \
    g_LDS.Item[_iLineOffset][_iPixelOffset].uBoth = float2ToUint32( float2( _RAWDataItem.fAO, _RAWDataItem.fDepth ) );

#define READ_FROM_LDS( _iLineOffset, _iPixelOffset, _RAWDataItem ) \
    float2 f2A = uintToFloat2( g_LDS.Item[_iLineOffset][_iPixelOffset].uBoth ); \
    _RAWDataItem.fAO = f2A.x; \
    _RAWDataItem.fDepth = f2A.y;

#endif

#endif


//--------------------------------------------------------------------------------------
// Get a Gaussian weight
//--------------------------------------------------------------------------------------
#define GAUSSIAN_WEIGHT( _fX, _fDeviation, _fWeight ) \
    _fWeight = 1.0f / sqrt( 2.0f * PI * _fDeviation * _fDeviation ); \
    _fWeight *= exp( -( _fX * _fX ) / ( 2.0f * _fDeviation * _fDeviation ) );


//--------------------------------------------------------------------------------------
// Sample from chosen input(s)
//--------------------------------------------------------------------------------------
#define SAMPLE_FROM_INPUT( _Sampler, _f2SamplePosition, _RAWDataItem ) \
    _RAWDataItem.fAO = g_txAO.SampleLevel( _Sampler, _f2SamplePosition, 0 ).x; \
    _RAWDataItem.fDepth = g_txDepth.SampleLevel( _Sampler, _f2SamplePosition, 0 ).x; \
    _RAWDataItem.fDepth = -g_aoInputData.m_CameraQTimesZNear / ( _RAWDataItem.fDepth - g_aoInputData.m_CameraQ );


//--------------------------------------------------------------------------------------
// Compute what happens at the kernels center
//--------------------------------------------------------------------------------------
#define KERNEL_CENTER( _KernelData, _iPixel, _iNumPixels, _O, _RAWDataItem ) \
    [unroll] for( _iPixel = 0; _iPixel < _iNumPixels; ++_iPixel ) { \
    _KernelData[_iPixel].fCenterAO = _RAWDataItem[_iPixel].fAO; \
    _KernelData[_iPixel].fCenterDepth = _RAWDataItem[_iPixel].fDepth; \
    GAUSSIAN_WEIGHT( 0, GAUSSIAN_DEVIATION, _KernelData[_iPixel].fWeight ) \
    _KernelData[_iPixel].fWeightSum = _KernelData[_iPixel].fWeight; \
    _O.fColor[_iPixel] = _KernelData[_iPixel].fCenterAO * _KernelData[_iPixel].fWeight; }


//--------------------------------------------------------------------------------------
// Compute what happens for each iteration of the kernel
//--------------------------------------------------------------------------------------
#define KERNEL_ITERATION( _iIteration, _KernelData, _iPixel, _iNumPixels, _O, _RAWDataItem ) \
    [unroll] for( _iPixel = 0; _iPixel < _iNumPixels; ++_iPixel ) { \
    GAUSSIAN_WEIGHT( ( _iIteration - KERNEL_RADIUS + ( 1.0f - 1.0f / float( STEP_SIZE ) ) ), GAUSSIAN_DEVIATION, _KernelData[_iPixel].fWeight ) \
    _KernelData[_iPixel].fWeight *= ( abs( _RAWDataItem[_iPixel].fDepth - _KernelData[_iPixel].fCenterDepth ) < g_aoInputData.m_DepthUpsampleThreshold ); \
    _O.fColor[_iPixel] += _RAWDataItem[_iPixel].fAO * _KernelData[_iPixel].fWeight; \
    _KernelData[_iPixel].fWeightSum += _KernelData[_iPixel].fWeight; }


//--------------------------------------------------------------------------------------
// Perform final weighting operation
//--------------------------------------------------------------------------------------
#define KERNEL_FINAL_WEIGHT( _KernelData, _iPixel, _iNumPixels, _O ) \
    [unroll] for( _iPixel = 0; _iPixel < _iNumPixels; ++_iPixel ) { \
    _O.fColor[_iPixel] = ( _KernelData[_iPixel].fWeightSum > 0.00001f ) ? ( _O.fColor[_iPixel] / _KernelData[_iPixel].fWeightSum ) : ( _KernelData[_iPixel].fCenterAO ); }


//--------------------------------------------------------------------------------------
// Output to chosen UAV
//--------------------------------------------------------------------------------------
#define KERNEL_OUTPUT( _i2Center, _i2Inc, _iPixel, _iNumPixels, _O, _KernelData ) \
    [unroll] for( _iPixel = 0; _iPixel < _iNumPixels; ++_iPixel ) \
    g_uavOutput[_i2Center + _iPixel * _i2Inc] = _O.fColor[_iPixel];


//--------------------------------------------------------------------------------------
// Include the filter kernel logic that uses the above macros
//--------------------------------------------------------------------------------------
#include "AOFX_SeparableFilter\\FilterKernel.hlsl"
#include "AOFX_SeparableFilter\\HorizontalFilter.hlsl"
#include "AOFX_SeparableFilter\\VerticalFilter.hlsl"


//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
