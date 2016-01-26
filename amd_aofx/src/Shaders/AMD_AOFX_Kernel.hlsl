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

#if ( AOFX_IMPLEMENTATION == AOFX_IMPLEMENTATION_CS )

struct CompressedPosition
{
# if AO_STORE_XY_LDS
  uint                                           position_xy;
# endif // AO_STORE_XY_LDS
  float                                          position_z;
};

groupshared CompressedPosition                   g_SharedCache[AO_GROUP_TEXEL_DIM][AO_GROUP_TEXEL_DIM];

// Helper function to load data from the LDS, given texel coord
// NOTE: X and Y are swapped around to ensure horizonatal reading across threads, this avoids
// LDS memory bank conflicts
float3 fetchPositionFromCache(uint2 screenCoord, uint2 cacheCoord, int2 layerIdx)
{
  float position_z = g_SharedCache[cacheCoord.y][cacheCoord.x].position_z;

# if AO_STORE_XY_LDS
  float2 position = uintToFloat2(g_SharedCache[cacheCoord.y][cacheCoord.x].position_xy);
# else
  // Convert screen coords to projection space XY
  position = screenCoord * g_cbAO.m_InputSizeRcp - float2(1.0f, 1.0f);
  position.x = position.x * g_cbAO.m_CameraTanHalfFovHorizontal * position_z;
  position.y = -position.y * g_cbAO.m_CameraTanHalfFovVertical   * position_z;
# endif // AO_STORE_XY_LDS

  return float3(position.x, position.y, position_z);
}

// Helper function to store data to the LDS, given texel coord
// NOTE: X and Y are swapped around to ensure horizonatal wrting across threads, this avoids
// LDS memory bank conflicts
void storePositionInCache(float3 position, uint2 cacheCoord)
{
# if AO_STORE_XY_LDS
  g_SharedCache[cacheCoord.y][cacheCoord.x].position_xy = float2ToUint32(position.xy);
# endif // AO_STORE_XY_LDS

  g_SharedCache[cacheCoord.y][cacheCoord.x].position_z = position.z;
}

#elif ( AOFX_IMPLEMENTATION == AOFX_IMPLEMENTATION_PS )

float3 fetchPositionFromCache(float2 screenCoord, uint2 cacheCoord, int2 layerIdx)
{
  return loadCameraSpacePositionT2D(screenCoord, layerIdx);
}

#endif // ( AOFX_IMPLEMENTATION == AOFX_IMPLEMENTATION_CS )

//==================================================================================================
// AO : Performs valley detection in Camera space, and uses the valley angle to scale occlusion.
//==================================================================================================
float kernelHDAO(float3 centerPosition, float2 screenCoord, int2 cacheCoord, uint randomIndex, uint2 layerIdx, uint layerIndex)
{
  if ( centerPosition.z > g_cbAO.m_ViewDistanceDiscard ) return 1.0;

  float centerDistance = length(centerPosition);
  float  occlusion = 0.0f;

#if ( AOFX_TAP_TYPE == AOFX_TAP_TYPE_RANDOM_CB )
  uint randomInput = randomIndex;
#elif ( AOFX_TAP_TYPE == AOFX_TAP_TYPE_RANDOM_SRV )
  uint randomInput = randomIndex * 32;
#endif

  [unroll]
  for ( uint uValley = 0; uValley < NUM_VALLEYS; uValley++ )
  {
    float3 direction[2];
    float3 position[2];
    float  distance[2];
    float2 distanceDelta;
    float2 compare;
    float  directionDot;

#if ( AOFX_TAP_TYPE == AOFX_TAP_TYPE_RANDOM_CB )
    int2 samplePattern = g_cbSamplePattern[randomInput][uValley].xy;
#elif ( AOFX_TAP_TYPE == AOFX_TAP_TYPE_RANDOM_SRV )
    int2 samplePattern = g_b1dSamplePattern.Load(randomInput + uValley).xy;
#else // ( AOFX_TAP_TYPE == AOFX_TAP_TYPE_FIXED )
    int2 samplePattern = g_SamplePattern[uValley].xy;
#endif // ( AOFX_TAP_TYPE == AOFX_TAP_TYPE_RANDOM_CB )

    // Sample
    position[0] = fetchPositionFromCache(screenCoord + samplePattern, cacheCoord + samplePattern, layerIdx);
    position[1] = fetchPositionFromCache(screenCoord - samplePattern, cacheCoord - samplePattern, layerIdx);

    // Compute distances
    distance[0] = length(position[0]);
    distance[1] = length(position[1]);

    // Detect valleys
    distanceDelta = centerDistance.xx - float2(distance[0], distance[1]);
    compare = saturate((g_cbAO.m_RejectRadius.xx - distanceDelta) * (g_cbAO.m_RecipFadeOutDist));
    compare = (distanceDelta > g_cbAO.m_AcceptRadius.xx) ? (compare) : (0.0f);

    // Compute dot product, to scale occlusion
    direction[0] = normalize(centerPosition - position[0]);
    direction[1] = normalize(centerPosition - position[1]);
    directionDot = saturate(dot(direction[0], direction[1]) + 0.9f) * 1.2f;

    // Accumulate weighted occlusion
    occlusion += compare.x * compare.y * directionDot * directionDot * directionDot;
  }

  // Finally calculate the AO occlusion value
  occlusion /= NUM_VALLEYS;
  occlusion *= g_cbAO.m_LinearIntensity;
  occlusion = 1.0f - saturate(occlusion);

  float weight = saturate((centerPosition.z - g_cbAO.m_ViewDistanceFade) / g_cbAO.m_FadeIntervalLength);
  return lerp(occlusion, 1.0f, weight);
}