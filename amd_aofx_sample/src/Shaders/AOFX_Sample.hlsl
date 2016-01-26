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

#include "../../../AMD_LIB/src/Shaders/AMD_FullscreenPass.hlsl"

struct ModelData
{
    float4x4          m_World;
    float4x4          m_WorldViewProjection;
    float4x4          m_WorldViewProjectionLight;
    float4            m_DiffuseColor;
    float4            m_AmbientColor;
};

struct CameraData
{
    float4x4          m_View;
    float4x4          m_Projection;
    float4x4          m_View_Inv;
    float4x4          m_Projection_Inv;
    float4x4          m_ViewProj;
    float4x4          m_ViewProj_Inv;

    float4            m_rtvSize;
    float4            m_Color;

    float4            m_Eye;
    float4            m_Direction;
    float4            m_Up;
    float4            m_FovAspectZNearZFar;
};

cbuffer CB_MODEL_DATA                                      : register(b0)
{
    ModelData         g_cbModel;
}

cbuffer CB_VIEWER_DATA : register(b1)
{
    CameraData        g_cbViewer;
}

cbuffer CB_LIGHT_ARRAY_DATA : register(b2)
{
    CameraData        g_cbLight[4];
}

Texture2D             g_t2dDiffuse : register(t0);
Texture2D             g_t2dNormal                          : register(t1);

SamplerState          g_ssPointClamp                       : register(s0);
SamplerState          g_ssBilinearClamp                    : register(s1);

// Lighting constants
static float4         g_LightDirectional[2] = { { 0.992f, 1.0, 0.880f, 0.0f }, { 0.595f, 0.6f, 0.528f, 0.0f } };
static float3         g_LightDirection[2]   = { { 1.705f, 5.557f, -9.380f }, { -5.947f, -5.342f, -5.733f } };
static float4         g_LightAmbient        = { 0.625f, 0.674f, 0.674f, 0.0f };
static float4         g_LightEye            = { 0.0f, 0.0f, 0.0f, 1.0f };

struct VS_RenderSceneInput
{
    float3            position                             : POSITION;
    float3            normal                               : NORMAL;
    float2            texCoord                             : TEXCOORD0;
    float3            tangent                              : TANGENT;
};

struct PS_RenderSceneInput
{
    float4            position                             : SV_Position;
    float2            texCoord                             : TEXCOORD0;
    float3            ws_normal                            : WS_NORMAL;   // World Space Normal
    float3            ws_tangent                           : WS_TANGENT;  // World Space Tangent
    float3            ws_position                          : WS_POSITION; // World Space Position
};

struct PS_RenderSceneOutput
{
    float4            color                                : SV_Target0;
    float4            ws_normal                            : SV_Target1;
};

//=================================================================================================
// This shader computes standard transform and lighting
//=================================================================================================
PS_RenderSceneInput vsRenderScene(VS_RenderSceneInput In)
{
    PS_RenderSceneInput Out = (PS_RenderSceneInput)0;

    // Transform the position from object space to homogeneous projection space
    Out.position = mul(float4(In.position, 1.0f), g_cbModel.m_WorldViewProjection);

    // Transform the normal, tangent and position from object space to world space
    Out.ws_position = mul(In.position, (float3x3)g_cbModel.m_World);
    Out.ws_normal = normalize(mul(In.normal, (float3x3)g_cbModel.m_World));
    Out.ws_tangent = normalize(mul(In.tangent, (float3x3)g_cbModel.m_World));

    // Pass through tex coords
    Out.texCoord = In.texCoord;

    return Out;
}

//=================================================================================================
// This shader outputs the pixel's color by passing through the lit diffuse material color
//=================================================================================================
PS_RenderSceneOutput psRenderScene(PS_RenderSceneInput In)
{
    PS_RenderSceneOutput Out = (PS_RenderSceneOutput)0;

    float4 lighting = g_LightAmbient;
    float3 halfAngle;
    float4 specularPower[2];
    float3 wsLightDirection[2];

    wsLightDirection[0] = normalize(mul(g_LightDirection[0], (float3x3)g_cbModel.m_World));
    wsLightDirection[1] = normalize(mul(g_LightDirection[1], (float3x3)g_cbModel.m_World));

    float4 diffuse = g_t2dDiffuse.Sample(g_ssBilinearClamp, In.texCoord);
    float specularMask = diffuse.a;
    float3 normal = g_t2dNormal.Sample(g_ssBilinearClamp, In.texCoord).xyz * 2.0f - float3(1.0f, 1.0f, 1.0f);
    float3 binormal = normalize(cross(In.ws_normal, In.ws_tangent));
    float3x3 tangentBasis = float3x3(binormal, In.ws_tangent, In.ws_normal);
    normal = normalize(mul(normal, tangentBasis));

    // Diffuse lighting
    lighting += saturate(dot(normal, wsLightDirection[0].xyz)) * g_LightDirectional[0];
    lighting += saturate(dot(normal, wsLightDirection[1].xyz)) * g_LightDirectional[1];

    // Calculate specular power
    float3 viewDirection = normalize(g_LightEye.xyz - In.ws_position);

    halfAngle = normalize(viewDirection + wsLightDirection[1].xyz);
    specularPower[0] = pow(saturate(dot(halfAngle, normal)), 32) * g_LightDirectional[0];

    halfAngle = normalize(viewDirection + wsLightDirection[1].xyz);
    specularPower[1] = pow(saturate(dot(halfAngle, normal)), 32) * g_LightDirectional[1];

    Out.color = lighting * diffuse + (specularPower[0] + specularPower[1]) * specularMask;
    Out.ws_normal = float4(normal.xyz, 0.0f);

    return Out;
}

//=================================================================================================
// Render Scene Color multipled by AO Result
//=================================================================================================
Texture2D      g_t2dScene                                  :  register (t0);
Texture2D      g_t2dAO                                     :  register (t1);

float4 psRenderSceneWithAO(PS_FullscreenInput In) : SV_Target
{
    float4 color = g_t2dScene.Sample(g_ssPointClamp, In.texCoord);
    float  ao = g_t2dAO.Sample(g_ssPointClamp, In.texCoord).x;

    return float4(color.xyz*ao, color.w);
}