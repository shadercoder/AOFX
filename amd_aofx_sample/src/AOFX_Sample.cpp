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

#include <string>

// DXUT includes
#include "DXUT.h"
#include "DXUTmisc.h"
#include "DXUTgui.h"
#include "DXUTCamera.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"

// AMD includes
#include "amd_ags.h"
#include "AMD_LIB.h"
#include "AMD_SDK.h"
#include "AMD_AOFX.h"

// Project includes
#include <DirectXMath.h>
using namespace DirectX;

#pragma warning( disable : 4100 ) // disable unreferenced formal parameter warnings
#pragma warning( disable : 4127 ) // disable conditional expression is constant warnings
#pragma warning( disable : 4201 ) // disable nameless struct/union warnings
#pragma warning( disable : 4238 ) // disable class rvalue used as lvalue warnings

__declspec(align(16))
struct float4
{
    union
    {
        XMVECTOR     v;
        float        f[4];
        struct { float x, y, z, w; };
    };

    float4(float ax = 0, float ay = 0, float az = 0, float aw = 0)
    {
        x = ax; y = ay; z = az; w = aw;
    }

    float4(XMVECTOR av) { v = av; }
    float4(XMVECTOR xyz, float aw) { v = xyz; w = aw; }

    inline operator XMVECTOR() const { return v; }
    inline operator const float*() const { return f; }
#if !defined(_XM_NO_INTRINSICS_) && defined(_XM_SSE_INTRINSICS_)
    inline operator __m128i() const { return _mm_castps_si128(v); }
    inline operator __m128d() const { return _mm_castps_pd(v); }
#endif
};

#define float4x4  XMMATRIX

__declspec(align(16))
struct CameraData
{
    float4x4                                     m_View;
    float4x4                                     m_Projection;
    float4x4                                     m_View_Inv;
    float4x4                                     m_Projection_Inv;
    float4x4                                     m_ViewProj;
    float4x4                                     m_ViewProj_Inv;

    float4                                       m_rtvSize;
    float4                                       m_Color;

    float4                                       m_Eye;
    float4                                       m_Direction;
    float4                                       m_Up;
    float4                                       m_FovAspectZNearZFar;
};

__declspec(align(16))
struct ModelData
{
    float4x4                                     m_World;
    float4x4                                     m_WorldViewProjection;
    float4x4                                     m_WorldViewProjectionLight;
    float4                                       m_DiffuseColor;
    float4                                       m_AmbientColor;
};


//-------------------------------------------------------------------------------------------------
// Global variables
//-------------------------------------------------------------------------------------------------

CFirstPersonCamera                               g_ViewerCamera;                    // A model viewing camera
CFirstPersonCamera                               g_LightCamera;                     // Dynamic Light
CFirstPersonCamera*                              g_pCurrentCamera = &g_ViewerCamera;
CameraData                                       g_ViewerData, g_LightData;

CDXUTDialogResourceManager                       g_DialogResourceManager;           // manager for shared resources of dialogs
CD3DSettingsDlg                                  g_SettingsDlg;                     // Device settings dialog
CDXUTTextHelper*                                 g_pTextHelper = NULL;

// Tank mesh scene
CDXUTSDKMesh                                     g_Mesh;
CDXUTSDKMesh*                                    g_MeshArray[] ={&g_Mesh};
XMMATRIX                                         g_MeshModelMatrix[AMD_ARRAY_SIZE(g_MeshArray)];

ID3D11InputLayout*                               g_pSceneIL = NULL;
ID3D11VertexShader*                              g_pSceneVS = NULL;
ID3D11PixelShader*                               g_pScenePS = NULL;

ID3D11VertexShader*                              g_pFullscreenPassVS = NULL;
ID3D11PixelShader*                               g_pFullscreenPassPS = NULL;

ID3D11PixelShader*                               g_pRenderSceneWithAOPS = NULL;

ID3D11Buffer*                                    g_pViewerCB = NULL;
ID3D11Buffer*                                    g_pLightCB = NULL;
ID3D11Buffer*                                    g_pModelCB = NULL;

ID3D11RasterizerState*                           g_pNoCullingSolidRS = NULL;
ID3D11RasterizerState*                           g_pBackCullingSolidRS = NULL;

ID3D11SamplerState*                              g_pPointClampSS = NULL;
ID3D11SamplerState*                              g_pLinearClampSS = NULL;

ID3D11DepthStencilState*                         g_pDepthTestLessDSS = NULL;
ID3D11DepthStencilState*                         g_pDepthTestLessEqualDSS = NULL;

ID3D11BlendState*                                g_pOpaqueBS = NULL;

AMD::Texture2D                                   g_AppColor;
AMD::Texture2D                                   g_AppNormal;
AMD::Texture2D                                   g_AppDepth;
AMD::Texture2D                                   g_AppResolvedColor;
AMD::Texture2D                                   g_AOResult;

unsigned int                                     g_uWidth = 1920;
unsigned int                                     g_uHeight = 1080;

AMD::AOFX_Desc                                   g_aoDesc;
int                                              g_aoLayer = 0;
bool                                             g_aoCapture = false;

//-------------------------------------------------------------------------------------------------
// AMD helper classes defined here
//-------------------------------------------------------------------------------------------------
static AMD::Magnify                              g_Magnify;
static AMD::MagnifyTool                          g_MagnifyTool;
static AMD::HUD                                  g_HUD;
static AMD::Sprite                               g_Sprite;

struct AMD_Slider
{
    AMD::Slider*   ui;
    float          fvalue;
    int            ivalue;
    AMD_Slider(int v)
    {
        ivalue = v;
    }

    AMD_Slider(float v)
    {
        fvalue = v;
    }
};

AMD_Slider                                       g_LayerDepthScale(1.0f);
AMD_Slider                                       g_LayerUpsampleThreshold(0.05f);
AMD_Slider                                       g_LayerPowIntensity(1);
AMD_Slider                                       g_LayerLinearIntensity(1.0f);
AMD_Slider                                       g_LayerRejectRadius(1.0f);
AMD_Slider                                       g_LayerAcceptRadius(1.0f);
AMD_Slider                                       g_LayerFadeOutDistance(1.0f);
AMD_Slider                                       g_LayerNormalScale(1.0f);
AMD_Slider                                       g_LayerDiscardDistance(1.0f);
AMD_Slider                                       g_LayerBilateralBlurRadius(0);

// Global boolean for HUD rendering
bool                                             g_RenderHUD = true;

//-------------------------------------------------------------------------------------------------
// UI control IDs
//-------------------------------------------------------------------------------------------------
enum IDC_VALUES
{
    IDC_TOGGLEFULLSCREEN,
    IDC_TOGGLEREF,
    IDC_CHANGEDEVICE,

    IDC_CHECKBOX_ENABLE_AO,
    IDC_CHECKBOX_DEBUG_AO,

    IDC_STATIC_AO_CURRENT_LAYER,
    IDC_COMBOBOX_AO_CURRENT_LAYER,

    IDC_STATIC_AOFX_SAMPLE_COUNT,
    IDC_COMBOBOX_AOFX_SAMPLE_COUNT,

    IDC_STATIC_AO_LAYER_TECH,
    IDC_COMBOBOX_AO_LAYER_TECH,

    IDC_STATIC_AO_RANDOM_TAPS,
    IDC_COMBOBOX_AO_RANDOM_TAPS,

    IDC_CHECKBOX_AO_ENABLE_NORMALS,
    IDC_CHECKBOX_AO_KERNEL_IMPLEMENTATION_PS,
    IDC_CHECKBOX_AO_UTILITY_IMPLEMENTATION_PS,

    IDC_SLIDER_AO_NORMAL_SCALE,
    IDC_SLIDER_AO_UPSAMPLE_THRESHOLD,
    IDC_SLIDER_AO_DEPTH_DOWNSCALE,
    IDC_SLIDER_AO_POW_INTENSITY,
    IDC_SLIDER_AO_LINEAR_INTENSITY,
    IDC_SLIDER_AO_ACCEPT_RADIUS,
    IDC_SLIDER_AO_REJECT_RADIUS,
    IDC_SLIDER_AO_ACCEPT_ANGLE,
    IDC_SLIDER_AO_FADE_OUT_DISTANCE,
    IDC_SLIDER_AO_BILATERAL_RADIUS,

    IDC_NUM_CONTROL_IDS,
};

//-------------------------------------------------------------------------------------------------
// Forward declarations
//-------------------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext);
void    CALLBACK OnKeyboard(UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext);
void    CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext);
void    CALLBACK OnFrameMove(double time, float elapsedTime, void* pUserContext);
bool    CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings, void* pUserContext);
bool    CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo, DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext);
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* pDevice, const DXGI_SURFACE_DESC* pSurfaceDesc, void* pUserContext);
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pDevice, IDXGISwapChain* pSwapChain, const DXGI_SURFACE_DESC* pSurfaceDesc, void* pUserContext);
void    CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext);
void    CALLBACK OnD3D11DestroyDevice(void* pUserContext);
void    CALLBACK OnD3D11FrameRender(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, double time, float elapsedTime, void* pUserContext);

void             CreateShaders(ID3D11Device * pDevice);
void             SetUIFromAO(unsigned int layer);
void             InitUI();
void             RenderText();
HRESULT          UpdateResolutionSlider(const int kMultiResIndex, CDXUTControl* pControl);

//-------------------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing
// loop. Idle time is used to render the scene.
//-------------------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) || defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    // DXUT will create and use the best device (either D3D9 or D3D11)
    // that is available on the system depending on which D3D callbacks are set below
    DXUTSetCallbackMsgProc(MsgProc);
    DXUTSetCallbackKeyboard(OnKeyboard);
    DXUTSetCallbackFrameMove(OnFrameMove);
    DXUTSetCallbackDeviceChanging(ModifyDeviceSettings);

    DXUTSetCallbackD3D11DeviceAcceptable(IsD3D11DeviceAcceptable);
    DXUTSetCallbackD3D11DeviceCreated(OnD3D11CreateDevice);
    DXUTSetCallbackD3D11SwapChainResized(OnD3D11ResizedSwapChain);
    DXUTSetCallbackD3D11SwapChainReleasing(OnD3D11ReleasingSwapChain);
    DXUTSetCallbackD3D11DeviceDestroyed(OnD3D11DestroyDevice);
    DXUTSetCallbackD3D11FrameRender(OnD3D11FrameRender);

    g_aoDesc.m_BilateralBlurRadius[0] = AMD::AOFX_BILATERAL_BLUR_RADIUS_NONE;
    g_aoDesc.m_BilateralBlurRadius[1] = AMD::AOFX_BILATERAL_BLUR_RADIUS_NONE;
    g_aoDesc.m_BilateralBlurRadius[2] = AMD::AOFX_BILATERAL_BLUR_RADIUS_NONE;
    g_aoDesc.m_LayerProcess[0] = AMD::AOFX_LAYER_PROCESS_DEINTERLEAVE_NONE;
    g_aoDesc.m_LayerProcess[1] = AMD::AOFX_LAYER_PROCESS_NONE;
    g_aoDesc.m_LayerProcess[2] = AMD::AOFX_LAYER_PROCESS_NONE;

    InitUI();

    DXUTInit(true, true, NULL);
    DXUTSetCursorSettings(true, true);
    DXUTCreateWindow(L"AMD AOFX v2.0");

    DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, g_uWidth, g_uHeight);

    DXUTMainLoop();

    return DXUTGetExitCode();
}

//-------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------
void SetCameraConstantBufferData(ID3D11DeviceContext* pContext,
    ID3D11Buffer*                                     pCB,
    CameraData*                                       pCameraData,
    CFirstPersonCamera*                               pCamera,
    unsigned int                                      nCount)
{
    D3D11_MAPPED_SUBRESOURCE MappedResource;

    for (unsigned int i = 0; i < nCount; i++)
    {
        CFirstPersonCamera & camera = pCamera[i];
        CameraData & cameraData = pCameraData[i];

        XMMATRIX  view = camera.GetViewMatrix();
        XMMATRIX  proj = camera.GetProjMatrix();
        XMMATRIX  viewproj = view * proj;
        XMMATRIX  view_inv = XMMatrixInverse(&XMMatrixDeterminant(view), view);
        XMMATRIX  proj_inv = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
        XMMATRIX  viewproj_inv = XMMatrixInverse(&XMMatrixDeterminant(viewproj), viewproj);

        cameraData.m_View = XMMatrixTranspose(view);
        cameraData.m_Projection = XMMatrixTranspose(proj);
        cameraData.m_View_Inv = XMMatrixTranspose(view_inv);
        cameraData.m_Projection_Inv = XMMatrixTranspose(proj_inv);
        cameraData.m_ViewProj = XMMatrixTranspose(viewproj);
        cameraData.m_ViewProj_Inv = XMMatrixTranspose(viewproj_inv);

        cameraData.m_Eye = camera.GetEyePt();
        cameraData.m_Direction = XMVector3Normalize(camera.GetLookAtPt() - camera.GetEyePt());
        cameraData.m_Up = camera.GetWorldUp();
        cameraData.m_FovAspectZNearZFar = float4(camera.GetFOV(), camera.GetAspect(), camera.GetNearClip(), camera.GetFarClip());
    }

    pContext->Map(pCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
    if (MappedResource.pData)
        memcpy(MappedResource.pData, pCameraData, sizeof(CameraData) * nCount);
    pContext->Unmap(pCB, 0);
}

void SetModelMatrices(ID3D11DeviceContext* pContext,
    ID3D11Buffer*                           pCB,
    const XMMATRIX&                         world,
    const XMMATRIX&                         viewProj)
{
    D3D11_MAPPED_SUBRESOURCE MappedResource;

    ModelData modelData;
    modelData.m_World = XMMatrixTranspose(world);
    modelData.m_WorldViewProjection = XMMatrixTranspose(world * viewProj);
    modelData.m_AmbientColor = float4(0.1f, 0.1f, 0.1f, 1.0f);
    modelData.m_DiffuseColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

    pContext->Map(pCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
    if (MappedResource.pData)
        memcpy(MappedResource.pData, &modelData, sizeof(modelData));
    pContext->Unmap(pCB, 0);
}

//-------------------------------------------------------------------------------------------------
// Render the scene (either for the main scene or the shadow map scene)
//-------------------------------------------------------------------------------------------------
void RenderScene(ID3D11DeviceContext* pContext,
    CDXUTSDKMesh*                     pMesh[],
    XMMATRIX*                         pModelMatrix,
    unsigned int                      nMeshCount,
    D3D11_VIEWPORT*                   pVP,           // ViewPort array
    unsigned int                      nVPCount,      // Viewport count
    D3D11_RECT*                       pSR,           // Scissor Rects array
    unsigned int                      nSRCount,      // Scissor rect count
    ID3D11RasterizerState*            pRS,           // Raster State
    ID3D11BlendState*                 pBS,           // Blend State
    float*                            pFactorBS,     // Blend state factor
    ID3D11DepthStencilState*          pDSS,          // Depth Stencil State
    unsigned int                      dssRef,        // Depth stencil state reference value
    ID3D11InputLayout*                pIL,           // Input Layout
    ID3D11VertexShader*               pVS,           // Vertex Shader
    ID3D11HullShader*                 pHS,           // Hull Shader
    ID3D11DomainShader*               pDS,           // Domain Shader
    ID3D11GeometryShader*             pGS,           // Geometry SHader
    ID3D11PixelShader*                pPS,           // Pixel Shader
    ID3D11Buffer*                     pModelCB,
    unsigned int                      nModelCBSlot,
    ID3D11Buffer**                    ppCB,          // Constant Buffer array
    unsigned int                      nCBStart,      // First slot to attach constant buffer array
    unsigned int                      nCBCount,      // Number of constant buffers in the array
    ID3D11SamplerState**              ppSS,          // Sampler State array
    unsigned int                      nSSStart,      // First slot to attach sampler state array
    unsigned int                      nSSCount,      // Number of sampler states in the array
    ID3D11ShaderResourceView**        ppSRV,         // Shader Resource View array
    unsigned int                      nSRVStart,     // First slot to attach sr views array
    unsigned int                      nSRVCount,     // Number of sr views in the array
    ID3D11RenderTargetView**          ppRTV,         // Render Target View array
    unsigned int                      nRTVCount,     // Number of rt views in the array
    ID3D11DepthStencilView*           pDSV,          // Depth Stencil View
    CFirstPersonCamera*               pViewerCamera)
{
    ID3D11RenderTargetView *   const pNullRTV[8] ={0};
    ID3D11ShaderResourceView * const pNullSRV[128] ={0};

    // Unbind anything that could be still bound on input or output
    // If this doesn't happen, DX Runtime will spam with warnings
    pContext->OMSetRenderTargets(AMD_ARRAY_SIZE(pNullRTV), pNullRTV, NULL);
    pContext->CSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);
    pContext->VSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);
    pContext->HSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);
    pContext->DSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);
    pContext->GSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);
    pContext->PSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);

    pContext->IASetInputLayout(pIL);

    pContext->VSSetShader(pVS, NULL, 0);
    pContext->HSSetShader(pHS, NULL, 0);
    pContext->DSSetShader(pDS, NULL, 0);
    pContext->GSSetShader(pGS, NULL, 0);
    pContext->PSSetShader(pPS, NULL, 0);

    if (nSSCount)
    {
        pContext->VSSetSamplers(nSSStart, nSSCount, ppSS);
        pContext->HSSetSamplers(nSSStart, nSSCount, ppSS);
        pContext->DSSetSamplers(nSSStart, nSSCount, ppSS);
        pContext->GSSetSamplers(nSSStart, nSSCount, ppSS);
        pContext->PSSetSamplers(nSSStart, nSSCount, ppSS);
    }

    if (nSRVCount)
    {
        pContext->VSSetShaderResources(nSRVStart, nSRVCount, ppSRV);
        pContext->HSSetShaderResources(nSRVStart, nSRVCount, ppSRV);
        pContext->DSSetShaderResources(nSRVStart, nSRVCount, ppSRV);
        pContext->GSSetShaderResources(nSRVStart, nSRVCount, ppSRV);
        pContext->PSSetShaderResources(nSRVStart, nSRVCount, ppSRV);
    }

    if (nCBCount)
    {
        pContext->VSSetConstantBuffers(nCBStart, nCBCount, ppCB);
        pContext->HSSetConstantBuffers(nCBStart, nCBCount, ppCB);
        pContext->DSSetConstantBuffers(nCBStart, nCBCount, ppCB);
        pContext->GSSetConstantBuffers(nCBStart, nCBCount, ppCB);
        pContext->PSSetConstantBuffers(nCBStart, nCBCount, ppCB);
    }

    pContext->OMSetRenderTargets(nRTVCount, ppRTV, pDSV);
    pContext->OMSetBlendState(pBS, pFactorBS, 0xf);
    pContext->OMSetDepthStencilState(pDSS, dssRef);
    pContext->RSSetState(pRS);
    pContext->RSSetScissorRects(nSRCount, pSR);
    pContext->RSSetViewports(nVPCount, pVP);

    // Setup the view matrices
    XMMATRIX view = pViewerCamera->GetViewMatrix();
    XMMATRIX proj = pViewerCamera->GetProjMatrix();
    XMMATRIX viewproj = view * proj;

    for (unsigned int mesh = 0; mesh < nMeshCount; mesh++)
    {
        SetModelMatrices(pContext, pModelCB, pModelMatrix[mesh], viewproj);

        pMesh[mesh]->Render(pContext, 0, 1, 2);
    }
}

void SetUIFromAO(unsigned int layer)
{
    g_HUD.m_GUI.GetComboBox(IDC_COMBOBOX_AO_LAYER_TECH)->SetSelectedByIndex(g_aoDesc.m_LayerProcess[layer] != -1 ? g_aoDesc.m_LayerProcess[layer] : AMD::AOFX_LAYER_PROCESS_COUNT);

    g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_AO_ENABLE_NORMALS)->SetChecked(g_aoDesc.m_NormalOption[layer] == AMD::AOFX_NORMAL_OPTION_READ_FROM_SRV);
    g_HUD.m_GUI.GetComboBox(IDC_COMBOBOX_AO_RANDOM_TAPS)->SetSelectedByIndex(g_aoDesc.m_TapType[layer]);
    g_HUD.m_GUI.GetComboBox(IDC_COMBOBOX_AOFX_SAMPLE_COUNT)->SetSelectedByIndex(g_aoDesc.m_SampleCount[layer]);


    //g_LayerUpsampleThreshold.ui->SetValue((float)g_aoDesc.m_DepthUpsampleThreshold[layer]);
    g_LayerDepthScale.ui->SetValue((float)g_aoDesc.m_MultiResLayerScale[layer]);
    g_LayerPowIntensity.ui->SetValue((int)g_aoDesc.m_PowIntensity[layer]);
    g_LayerLinearIntensity.ui->SetValue((float)g_aoDesc.m_LinearIntensity[layer]);

    g_LayerRejectRadius.ui->SetValue((float)g_aoDesc.m_RejectRadius[layer]);
    g_LayerAcceptRadius.ui->SetValue((float)g_aoDesc.m_AcceptRadius[layer]);

    g_LayerFadeOutDistance.ui->SetValue((float)g_aoDesc.m_RecipFadeOutDist[layer]);
    g_LayerNormalScale.ui->SetValue((float)g_aoDesc.m_NormalScale[layer]);
    g_LayerDiscardDistance.ui->SetValue((float)g_aoDesc.m_ViewDistanceDiscard[layer]);
    g_LayerBilateralBlurRadius.ui->SetValue((int)g_aoDesc.m_BilateralBlurRadius[layer] + 1);
}

//-------------------------------------------------------------------------------------------------
// Initialize the app
//-------------------------------------------------------------------------------------------------
void InitUI()
{
    CDXUTComboBox *pCombo = NULL;
    D3DCOLOR DlgColor = 0x88888888; // Semi-transparent background for the dialog

    g_SettingsDlg.Init(&g_DialogResourceManager);
    g_HUD.m_GUI.Init(&g_DialogResourceManager);
    g_HUD.m_GUI.SetBackgroundColors(DlgColor);
    g_HUD.m_GUI.SetCallback(OnGUIEvent);

    int iY = AMD::HUD::iElementDelta;

    g_HUD.m_GUI.AddButton(IDC_TOGGLEFULLSCREEN, L"Toggle full screen", AMD::HUD::iElementOffset, iY, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
    g_HUD.m_GUI.AddButton(IDC_TOGGLEREF, L"Toggle REF (F3)", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, VK_F3);
    g_HUD.m_GUI.AddButton(IDC_CHANGEDEVICE, L"Change device (F2)", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, VK_F2);

    iY += AMD::HUD::iGroupDelta;

    g_HUD.m_GUI.AddCheckBox(IDC_CHECKBOX_ENABLE_AO, L"Enable AO", AMD::HUD::iElementOffset, iY, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, true);
    g_HUD.m_GUI.AddCheckBox(IDC_CHECKBOX_DEBUG_AO, L"Render AO", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, false);

    g_HUD.m_GUI.AddCheckBox(IDC_CHECKBOX_AO_KERNEL_IMPLEMENTATION_PS, L"Use PS Kernel",
        AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight,
        (g_aoDesc.m_Implementation & AMD::AOFX_IMPLEMENTATION_MASK_KERNEL_PS) != 0);
    g_HUD.m_GUI.AddCheckBox(IDC_CHECKBOX_AO_UTILITY_IMPLEMENTATION_PS, L"Use PS Utility",
        AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight,
        (g_aoDesc.m_Implementation & AMD::AOFX_IMPLEMENTATION_MASK_UTILITY_PS) != 0);

    g_HUD.m_GUI.AddStatic(IDC_STATIC_AO_CURRENT_LAYER, L"Current AO Layer:", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
    g_HUD.m_GUI.AddComboBox(IDC_COMBOBOX_AO_CURRENT_LAYER, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 0, true, &pCombo);
    if (pCombo)
    {
        pCombo->SetDropHeight(35);
        pCombo->AddItem(L"0", NULL);
        pCombo->AddItem(L"1", NULL);
        pCombo->AddItem(L"2", NULL);
        pCombo->SetSelectedByIndex(g_aoLayer);
    }

    g_HUD.m_GUI.AddStatic(IDC_STATIC_AOFX_SAMPLE_COUNT, L"AO Sample Count:", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
    g_HUD.m_GUI.AddComboBox(IDC_COMBOBOX_AOFX_SAMPLE_COUNT, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 0, true, &pCombo);
    if (pCombo)
    {
        pCombo->SetDropHeight(35);
        pCombo->AddItem(L"8 ", NULL);
        pCombo->AddItem(L"16", NULL);
        pCombo->AddItem(L"24", NULL);
        pCombo->AddItem(L"32", NULL);
        int index = (int)g_aoDesc.m_SampleCount[g_aoLayer];
        pCombo->SetSelectedByIndex(index);
    }

    g_HUD.m_GUI.AddCheckBox(IDC_CHECKBOX_AO_ENABLE_NORMALS, L"Use Normals", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, g_aoDesc.m_NormalOption[g_aoLayer] != AMD::AOFX_NORMAL_OPTION_NONE);

    g_HUD.m_GUI.AddStatic(IDC_STATIC_AO_RANDOM_TAPS, L"Sample Tap Type:", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
    g_HUD.m_GUI.AddComboBox(IDC_COMBOBOX_AO_RANDOM_TAPS, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 0, true, &pCombo);
    if (pCombo)
    {
        pCombo->SetDropHeight(35);
        pCombo->AddItem(L"Fixed Taps", NULL);
        pCombo->AddItem(L"Random Taps CB", NULL);
        pCombo->AddItem(L"Random Taps SRV", NULL);
        int index = (int)g_aoDesc.m_TapType[g_aoLayer];
        pCombo->SetSelectedByIndex(index);
    }

    g_HUD.m_GUI.AddStatic(IDC_STATIC_AO_LAYER_TECH, L"AO Technique:", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
    g_HUD.m_GUI.AddComboBox(IDC_COMBOBOX_AO_LAYER_TECH, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 0, true, &pCombo);
    if (pCombo)
    {
        pCombo->SetDropHeight(35);
        pCombo->AddItem(L"Deinterleave 1x", NULL);
        pCombo->AddItem(L"Deinterleave 2x", NULL);
        pCombo->AddItem(L"Deinterleave 4x", NULL);
        pCombo->AddItem(L"Deinterleave 8x", NULL);
        pCombo->AddItem(L"Disabled", NULL);
        int index = (int)g_aoDesc.m_LayerProcess[g_aoLayer];
        pCombo->SetSelectedByIndex(index);
    }

    //g_LayerUpsampleThreshold.ui = new AMD::Slider(g_HUD.m_GUI, IDC_SLIDER_AO_UPSAMPLE_THRESHOLD, iY, L"Upsample Threshold", 0.0001f, 0.1f, 0.0001f, g_LayerUpsampleThreshold.fvalue); // disable upsample threshold value because it's not used in the lib
    g_LayerDepthScale.ui = new AMD::Slider(g_HUD.m_GUI, IDC_SLIDER_AO_DEPTH_DOWNSCALE, iY, L"Depth Downscaling", 0.01f, 1.0f, 0.01f, g_LayerDepthScale.fvalue);
    g_LayerPowIntensity.ui = new AMD::Slider(g_HUD.m_GUI, IDC_SLIDER_AO_POW_INTENSITY, iY, L"Pow Intensity", 1, 50, g_LayerPowIntensity.ivalue);
    g_LayerLinearIntensity.ui = new AMD::Slider(g_HUD.m_GUI, IDC_SLIDER_AO_LINEAR_INTENSITY, iY, L"Linear Intensity", 0.01f, 1.0f, 0.01f, g_LayerLinearIntensity.fvalue);

    g_LayerRejectRadius.ui = new AMD::Slider(g_HUD.m_GUI, IDC_SLIDER_AO_REJECT_RADIUS, iY, L"Rejection Radius", 0.01f, 2.0f, 0.01f, g_LayerRejectRadius.fvalue);
    g_LayerAcceptRadius.ui = new AMD::Slider(g_HUD.m_GUI, IDC_SLIDER_AO_ACCEPT_RADIUS, iY, L"Accept Radius", 0.01f, 1.0f, 0.01f, g_LayerAcceptRadius.fvalue);

    g_LayerFadeOutDistance.ui = new AMD::Slider(g_HUD.m_GUI, IDC_SLIDER_AO_FADE_OUT_DISTANCE, iY, L"Fade Out Distance", 0.01f, 100.0f, 0.01f, g_LayerFadeOutDistance.fvalue);
    g_LayerNormalScale.ui = new AMD::Slider(g_HUD.m_GUI, IDC_SLIDER_AO_NORMAL_SCALE, iY, L"Normal Scale", 0.01f, 1.0f, 0.01f, g_LayerNormalScale.fvalue);
    g_LayerDiscardDistance.ui = new AMD::Slider(g_HUD.m_GUI, IDC_SLIDER_AO_ACCEPT_ANGLE, iY, L"Discard Distance", 0.01f, 100.0f, 0.01f, g_LayerDiscardDistance.fvalue);
    g_LayerBilateralBlurRadius.ui = new AMD::Slider(g_HUD.m_GUI, IDC_SLIDER_AO_BILATERAL_RADIUS, iY, L"Bilateral Blur Radius", 0, AMD::AOFX_BILATERAL_BLUR_RADIUS_COUNT, g_LayerBilateralBlurRadius.ivalue);

    SetUIFromAO(g_aoLayer);

    iY += AMD::HUD::iGroupDelta;

    // Add the magnify tool UI to our HUD
    g_MagnifyTool.InitApp(&g_HUD.m_GUI, iY);
}


//-------------------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for
// efficient text rendering.
//-------------------------------------------------------------------------------------------------
void RenderText()
{
    g_pTextHelper->Begin();
    g_pTextHelper->SetInsertionPos(5, 5);
    g_pTextHelper->SetForegroundColor(XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f));
    g_pTextHelper->DrawTextLine(DXUTGetFrameStats(DXUTIsVsyncEnabled()));
    g_pTextHelper->DrawTextLine(DXUTGetDeviceStats());

    static float timeAvgAO = 0.0f;
    static float timeAOFX = 0;
    static int   count = 0;

    float timeSceneRendering = (float)TIMER_GetTime(Gpu, L"Scene Rendering") * 1000.0f;
    float timeAO = (float)TIMER_GetTime(Gpu, L"AMD_AOFX") * 1000.0f;
    float timeTotal = timeSceneRendering + timeAO;
    timeAvgAO += timeAO;

    if (count == 250)
    {
        timeAOFX = timeAvgAO / 250.0f;
        timeAvgAO = 0.0f;
        count = 0;
    }

    WCHAR wcbuf[256];
    swprintf_s(wcbuf, 256, L"Cost in milliseconds( Total = %.2f, Scene Rendering = %.2f, AO = %.2f )", timeTotal, timeSceneRendering, timeAOFX);
    g_pTextHelper->DrawTextLine(wcbuf);
    g_pTextHelper->SetInsertionPos(5, DXUTGetDXGIBackBufferSurfaceDesc()->Height - AMD::HUD::iElementDelta);
    g_pTextHelper->DrawTextLine(L"Toggle GUI    : F1");
    g_pTextHelper->End();

    count++;
}

//-------------------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//-------------------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdapterInfo,
    UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
    DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext)
{
    return true;
}

//-------------------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//-------------------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* pDevice,
    const DXGI_SURFACE_DESC*                        pSurfaceDesc,
    void*                                           pUserContext)
{
    HRESULT hr;

    ID3D11DeviceContext* pContext = DXUTGetD3D11DeviceContext();
    V_RETURN(g_DialogResourceManager.OnD3D11CreateDevice(pDevice, pContext));
    V_RETURN(g_SettingsDlg.OnD3D11CreateDevice(pDevice));
    g_pTextHelper = new CDXUTTextHelper(pDevice, pContext, &g_DialogResourceManager, 15);

    CD3D11_DEFAULT defaultDesc;

    // Setup constant buffers
    D3D11_BUFFER_DESC bufferDesc;

    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc.MiscFlags = 0;
    bufferDesc.ByteWidth = sizeof(CameraData);
    V_RETURN(pDevice->CreateBuffer(&bufferDesc, NULL, &g_pViewerCB));
    DXUT_SetDebugName(g_pViewerCB, "g_pViewerCB");

    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc.MiscFlags = 0;
    bufferDesc.ByteWidth = sizeof(ModelData);
    V_RETURN(pDevice->CreateBuffer(&bufferDesc, NULL, &g_pModelCB));
    DXUT_SetDebugName(g_pModelCB, "g_pModelCB");

    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc.MiscFlags = 0;
    bufferDesc.ByteWidth = sizeof(g_LightData);
    V_RETURN(pDevice->CreateBuffer(&bufferDesc, NULL, &g_pLightCB));
    DXUT_SetDebugName(g_pLightCB, "g_pLightCB");

    // Create blend states
    CD3D11_BLEND_DESC blendDesc(defaultDesc);
    pDevice->CreateBlendState((const D3D11_BLEND_DESC*)&blendDesc, &g_pOpaqueBS);

    // Create sampler states for point and linear
    CD3D11_SAMPLER_DESC samplerDesc(defaultDesc);
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    V_RETURN(pDevice->CreateSamplerState(&samplerDesc, &g_pPointClampSS));
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    V_RETURN(pDevice->CreateSamplerState(&samplerDesc, &g_pLinearClampSS));

    CD3D11_RASTERIZER_DESC rsDesc(defaultDesc);
    rsDesc.CullMode = D3D11_CULL_NONE;
    rsDesc.FillMode = D3D11_FILL_SOLID;
    pDevice->CreateRasterizerState(&rsDesc, &g_pNoCullingSolidRS);
    rsDesc.CullMode = D3D11_CULL_BACK;
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.ScissorEnable = false;
    pDevice->CreateRasterizerState(&rsDesc, &g_pBackCullingSolidRS);

    // regular DSS
    CD3D11_DEPTH_STENCIL_DESC dssDesc(defaultDesc);
    pDevice->CreateDepthStencilState(&dssDesc, &g_pDepthTestLessDSS);
    dssDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    pDevice->CreateDepthStencilState(&dssDesc, &g_pDepthTestLessEqualDSS);

    // Create other render resources here
    // Setup the camera's view parameters
    XMFLOAT3 vecEye(-8.32327f, 7.48591f, -2.21140f);
    XMFLOAT3 vecAt(-7.61476f, 6.91929f, -1.79074f);
    g_ViewerCamera.SetRotateButtons(true, false, false);
    g_ViewerCamera.SetEnablePositionMovement(true);
    g_ViewerCamera.SetViewParams(XMLoadFloat3(&vecEye), XMLoadFloat3(&vecAt));
    g_ViewerCamera.SetScalers(0.005f, 100.0f);

    // Load the scene mesh
    g_Mesh.Create(pDevice, L"tank\\tankscene.sdkmesh", false);
    g_MeshModelMatrix[0] = XMMatrixIdentity();

    // Create AMD_SDK resources here
    g_HUD.OnCreateDevice(pDevice);
    g_MagnifyTool.OnCreateDevice(pDevice);
    TIMER_Init(pDevice);
    g_Sprite.OnCreateDevice(pDevice);

    // akharlamov: AMD::AOFX_Desc update
    g_aoDesc.m_pDevice = pDevice;
    g_aoDesc.m_pDeviceContext = pContext;
    AOFX_Initialize(g_aoDesc);

    CreateShaders(pDevice);

    return S_OK;
}

//-------------------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//-------------------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pDevice,
    IDXGISwapChain*                                     pSwapChain,
    const DXGI_SURFACE_DESC*                            pSurfaceDesc,
    void*                                               pUserContext)
{
    HRESULT hr;

    V_RETURN(g_DialogResourceManager.OnD3D11ResizedSwapChain(pDevice, pSurfaceDesc));
    V_RETURN(g_SettingsDlg.OnD3D11ResizedSwapChain(pDevice, pSurfaceDesc));

    g_uWidth = pSurfaceDesc->Width;
    g_uHeight = pSurfaceDesc->Height;

    // Setup the camera's projection parameters
    float fAspectRatio = pSurfaceDesc->Width / (FLOAT)pSurfaceDesc->Height;
    g_ViewerCamera.SetProjParams(XM_PI / 4, fAspectRatio, 0.1f, 1000.0f);

    // Set the location and size of the AMD standard HUD
    g_HUD.m_GUI.SetLocation(pSurfaceDesc->Width - AMD::HUD::iDialogWidth, 0);
    g_HUD.m_GUI.SetSize(AMD::HUD::iDialogWidth, pSurfaceDesc->Height);
    g_HUD.OnResizedSwapChain(pSurfaceDesc);

    g_Sprite.OnResizedSwapChain(pSurfaceDesc);

    // Magnify tool will capture from the color buffer
    g_MagnifyTool.OnResizedSwapChain(pDevice, pSwapChain, pSurfaceDesc, pUserContext, pSurfaceDesc->Width - AMD::HUD::iDialogWidth, 0);
    D3D11_RENDER_TARGET_VIEW_DESC RTDesc;
    ID3D11Resource* pTempRTResource;
    DXUTGetD3D11RenderTargetView()->GetResource(&pTempRTResource);
    DXUTGetD3D11RenderTargetView()->GetDesc(&RTDesc);
    g_MagnifyTool.SetSourceResources(pTempRTResource, RTDesc.Format, g_uWidth, g_uHeight, DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Count);
    g_MagnifyTool.SetPixelRegion(128);
    g_MagnifyTool.SetScale(5);
    SAFE_RELEASE(pTempRTResource);

    g_AppDepth.Release();
    hr = g_AppDepth.CreateSurface(DXUTGetD3D11Device(),
        pSurfaceDesc->Width, pSurfaceDesc->Height,
        pSurfaceDesc->SampleDesc.Count, 1, 1,
        DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_D32_FLOAT,
        DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, D3D11_USAGE_DEFAULT, false, 0, NULL, NULL, AGS_AFR_TRANSFER_DISABLE);

    g_AppNormal.Release();
    hr = g_AppNormal.CreateSurface(DXUTGetD3D11Device(),
        pSurfaceDesc->Width, pSurfaceDesc->Height,
        pSurfaceDesc->SampleDesc.Count, 1, 1,
        DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,
        DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN,
        D3D11_USAGE_DEFAULT, false, 0, NULL, NULL, AGS_AFR_TRANSFER_DISABLE);
    assert(S_OK == hr);

    g_AOResult.Release();
    hr = g_AOResult.CreateSurface(DXUTGetD3D11Device(),
        pSurfaceDesc->Width, pSurfaceDesc->Height, 1, 1, 1,
        DXGI_FORMAT_R8G8B8A8_UNORM, // THE FORMAT IS ONLY SELECTED AS RGBA8_UNORM (R8 would normally be enough)
        DXGI_FORMAT_R8G8B8A8_UNORM, // SO THAT THE OUTPUT CAN LOOK BLACK AND WHITE
        DXGI_FORMAT_R8G8B8A8_UNORM, // AND TO DEMONSTRATE THE USE OF OUTPUT CHANNELS
        DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN,
        D3D11_USAGE_DEFAULT, false, 0, NULL, NULL, AGS_AFR_TRANSFER_DISABLE);
    assert(S_OK == hr);

    g_AppColor.Release();
    hr = g_AppColor.CreateSurface(DXUTGetD3D11Device(),
        pSurfaceDesc->Width, pSurfaceDesc->Height,
        pSurfaceDesc->SampleDesc.Count, 1, 1,
        DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN,
        D3D11_USAGE_DEFAULT, false, 0, NULL, NULL, AGS_AFR_TRANSFER_DISABLE);
    assert(S_OK == hr);

    if (pSurfaceDesc->SampleDesc.Count > 1)
    {
        // Resolved scene buffer creation
        g_AppResolvedColor.Release();
        hr = g_AppResolvedColor.CreateSurface(DXUTGetD3D11Device(),
            pSurfaceDesc->Width, pSurfaceDesc->Height, 1, 1, 1,
            DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM,
            DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN,
            D3D11_USAGE_DEFAULT, false, 0, NULL, NULL, AGS_AFR_TRANSFER_DISABLE);
        assert(S_OK == hr);
    }

    // akharlamov: AMD::AOFX_Desc update
    g_aoDesc.m_pDevice = DXUTGetD3D11Device();
    g_aoDesc.m_pDeviceContext = DXUTGetD3D11DeviceContext();
    g_aoDesc.m_InputSize.x = pSurfaceDesc->Width;
    g_aoDesc.m_InputSize.y = pSurfaceDesc->Height;
    g_aoDesc.m_Camera.m_Fov = g_ViewerCamera.GetFOV();
    g_aoDesc.m_Camera.m_Aspect = g_ViewerCamera.GetAspect();
    g_aoDesc.m_Camera.m_NearPlane = g_ViewerCamera.GetNearClip();
    g_aoDesc.m_Camera.m_FarPlane = g_ViewerCamera.GetFarClip();
    AOFX_Resize(g_aoDesc);

    return S_OK;
}

//-------------------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//-------------------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender(ID3D11Device* pDevice,
    ID3D11DeviceContext*                        pContext,
    double                                      time,
    float                                       elapsedTime,
    void*                                       pUserContext)
{
    D3D11_RECT*                pNullSR = NULL;
    ID3D11HullShader*          pNullHS = NULL;
    ID3D11DomainShader*        pNullDS = NULL;
    ID3D11GeometryShader*      pNullGS = NULL;
    ID3D11DepthStencilView*    pNullDSV = NULL;
    ID3D11DepthStencilState*   pNullDSS = NULL;

    ID3D11Buffer*              pNullCB = NULL;

    ID3D11RenderTargetView*    pOriginalRTV = NULL;
    ID3D11DepthStencilView*    pOriginalDSV = NULL;

    static bool                bCapture = false;

    float4 lightBlue(0.176f, 0.196f, 0.667f, 0.000f);
    float4 white(1.000f, 1.000f, 1.000f, 1.000f);
    float4 grey(0.500f, 0.500f, 0.500f, 0.500f);

    if (GetAsyncKeyState(VK_SNAPSHOT) & 0x8000)
        g_aoCapture = true; // capture lights until the end of this frame

    // Reset the timer at start of frame
    TIMER_Reset();

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if (g_SettingsDlg.IsActive())
    {
        g_SettingsDlg.OnRender(elapsedTime);
        return;
    }

    // Store off original render target, this is the back buffer of the swap chain
    pContext->OMGetRenderTargets(1, &pOriginalRTV, &pOriginalDSV);

    // Clear the render targets & depth stencil
    pContext->ClearRenderTargetView(g_AppColor._rtv, lightBlue);
    pContext->ClearRenderTargetView(g_AppNormal._rtv, grey);
    pContext->ClearDepthStencilView(g_AppDepth._dsv, D3D11_CLEAR_DEPTH, 1.0, 0);

    SetCameraConstantBufferData(pContext, g_pViewerCB, &g_ViewerData, &g_ViewerCamera, 1);
    SetCameraConstantBufferData(pContext, g_pLightCB, &g_LightData, &g_LightCamera, 1);

    TIMER_Begin(0, L"Scene Rendering");
    {
        ID3D11SamplerState*        pSS[] ={g_pPointClampSS, g_pLinearClampSS};
        ID3D11Buffer*              pCB[] ={g_pModelCB, g_pViewerCB, g_pLightCB};
        ID3D11RenderTargetView*    pRTV[] ={g_AppColor._rtv, g_AppNormal._rtv};
        ID3D11ShaderResourceView*  pSRV[] ={NULL};

        RenderScene(pContext,
            g_MeshArray, g_MeshModelMatrix, AMD_ARRAY_SIZE(g_MeshArray),
            &CD3D11_VIEWPORT(0.0f, 0.0f, (float)g_uWidth, (float)g_uHeight), 1,
            pNullSR, 0,
            g_pBackCullingSolidRS,
            g_pOpaqueBS, white.f,
            g_pDepthTestLessDSS, 0,
            g_pSceneIL,
            g_pSceneVS, pNullHS, pNullDS, pNullGS, g_pScenePS,
            g_pModelCB, 0,
            pCB, 0, AMD_ARRAY_SIZE(pCB),
            pSS, 0, AMD_ARRAY_SIZE(pSS),
            pSRV, 4, AMD_ARRAY_SIZE(pSRV),
            pRTV, AMD_ARRAY_SIZE(pRTV), g_AppDepth._dsv,
            &g_ViewerCamera);
    }
    TIMER_End();

    // Render the AO buffer
    if (g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_AO)->GetChecked())
    {
        TIMER_Begin(0, L"AMD_AOFX");
        {
            g_aoDesc.m_Camera.m_Fov = g_ViewerCamera.GetFOV();
            g_aoDesc.m_Camera.m_Aspect = g_ViewerCamera.GetAspect();
            g_aoDesc.m_Camera.m_NearPlane = g_ViewerCamera.GetNearClip();
            g_aoDesc.m_Camera.m_FarPlane = g_ViewerCamera.GetFarClip();

            g_aoDesc.m_InputSize.x = DXUTGetDXGIBackBufferSurfaceDesc()->Width;
            g_aoDesc.m_InputSize.y = DXUTGetDXGIBackBufferSurfaceDesc()->Height;

            g_aoDesc.m_pDepthSRV = g_AppDepth._srv;
            g_aoDesc.m_pNormalSRV = g_AppNormal._srv;

            g_aoDesc.m_pOutputRTV = g_AOResult._rtv;
            g_aoDesc.m_pDeviceContext = pContext;
            g_aoDesc.m_OutputChannelsFlag = 8 | 4 | 2 | 1;
            AOFX_Render(g_aoDesc);

            // Capturing is currently disabled
#if defined(AMD_AOFX_DEBUG)
            if (g_aoCapture)
            {
                AOFX_DebugSerialize(g_aoDesc, "aofx");
            }
#endif
            g_aoCapture = false;
        }
        TIMER_End();
    }

    // If MSAA is enabled then we need to resolve the scene buffer
    if (DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Count > 1)
    {
        pContext->ResolveSubresource(g_AppResolvedColor._t2d, 0, g_AppColor._t2d, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    }

    ///////////////////////////////////////////////////////////////////////////
    // Combine the scene with the AO buffer
    if (g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_AO)->GetChecked())
    {
        if (g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_DEBUG_AO)->GetChecked())
        {
            ID3D11ShaderResourceView * pSRVs[] ={g_AOResult._srv};
            AMD::RenderFullscreenPass(pContext,
                CD3D11_VIEWPORT(0.0f, 0.0f, (float)g_uWidth, (float)g_uHeight),
                g_pFullscreenPassVS, g_pFullscreenPassPS,
                pNullSR, 0, &pNullCB, 0,
                &g_pPointClampSS, 1,
                pSRVs, 1,
                &pOriginalRTV, 1,
                NULL, 0, 0,
                pNullDSV, pNullDSS, 0,
                g_pOpaqueBS, g_pNoCullingSolidRS);
        }
        else // NOT g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_DEBUG_AO )->GetChecked()
        {
            ID3D11ShaderResourceView * pSRVs[] ={(DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Count > 1) ? g_AppResolvedColor._srv : g_AppColor._srv, g_AOResult._srv};
            AMD::RenderFullscreenPass(pContext,
                CD3D11_VIEWPORT(0.0f, 0.0f, (float)g_uWidth, (float)g_uHeight),
                g_pFullscreenPassVS, g_pRenderSceneWithAOPS,
                pNullSR, 0, &pNullCB, 0,
                &g_pPointClampSS, 1,
                pSRVs, 2,
                &pOriginalRTV, 1,
                NULL, 0, 0,
                pNullDSV, pNullDSS, 0,
                g_pOpaqueBS, g_pNoCullingSolidRS);
        }
    }
    else // NOT g_HUD.m_GUI.GetCheckBox( IDC_CHECKBOX_ENABLE_AO )->GetChecked()
    {
        ID3D11ShaderResourceView * pSRVs[] ={(DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Count > 1) ? g_AppResolvedColor._srv : g_AppColor._srv};

        AMD::RenderFullscreenPass(pContext,
            CD3D11_VIEWPORT(0.0f, 0.0f, (float)g_uWidth, (float)g_uHeight),
            g_pFullscreenPassVS, g_pFullscreenPassPS,
            pNullSR, 0, &pNullCB, 0,
            &g_pPointClampSS, 1,
            pSRVs, 1,
            &pOriginalRTV, 1,
            NULL, 0, 0,
            pNullDSV, pNullDSS, 0,
            g_pOpaqueBS, g_pNoCullingSolidRS);
    }

    // Rebind to original back buffer and depth buffer
    pContext->OMSetRenderTargets(1, &pOriginalRTV, pOriginalDSV);
    SAFE_RELEASE(pOriginalRTV);
    SAFE_RELEASE(pOriginalDSV);

    DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"HUD / Stats");

    // Render the HUD
    if (g_RenderHUD)
    {
        g_MagnifyTool.Render();
        g_HUD.OnRender(elapsedTime);
    }

    RenderText();

    DXUT_EndPerfEvent();

    static DWORD dwTimefirst = GetTickCount();
    if (GetTickCount() - dwTimefirst > 5000)
    {
        OutputDebugString(DXUTGetFrameStats(DXUTIsVsyncEnabled()));
        OutputDebugString(L"\n");
        dwTimefirst = GetTickCount();
    }
}

//-------------------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain
//-------------------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext)
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();

    g_AppDepth.Release();
    g_AppColor.Release();
    g_AppResolvedColor.Release();
    g_AppNormal.Release();
    g_AOResult.Release();
}

//-------------------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice
//-------------------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice(void* pUserContext)
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_SettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE(g_pTextHelper);

    g_Mesh.Destroy();

    SAFE_RELEASE(g_pOpaqueBS);

    SAFE_RELEASE(g_pModelCB);
    SAFE_RELEASE(g_pViewerCB);
    SAFE_RELEASE(g_pLightCB);

    SAFE_RELEASE(g_pSceneIL);
    SAFE_RELEASE(g_pSceneVS);
    SAFE_RELEASE(g_pFullscreenPassVS);
    SAFE_RELEASE(g_pScenePS);

    SAFE_RELEASE(g_pRenderSceneWithAOPS);
    SAFE_RELEASE(g_pFullscreenPassPS);

    g_AppDepth.Release();
    g_AppColor.Release();
    g_AppResolvedColor.Release();
    g_AppNormal.Release();
    g_AOResult.Release();

    SAFE_RELEASE(g_pPointClampSS);
    SAFE_RELEASE(g_pLinearClampSS);

    SAFE_RELEASE(g_pBackCullingSolidRS);
    SAFE_RELEASE(g_pNoCullingSolidRS);

    SAFE_RELEASE(g_pDepthTestLessDSS);
    SAFE_RELEASE(g_pDepthTestLessEqualDSS);

    // Destroy AMD_SDK resources here
    AOFX_Release(g_aoDesc);

    g_HUD.OnDestroyDevice();
    g_MagnifyTool.OnDestroyDevice();
    TIMER_Destroy();
    g_Sprite.OnDestroyDevice();
}

//-------------------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D11 device, allowing the app to modify the device settings as needed
//-------------------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings, void* pUserContext)
{
    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if (s_bFirstTime)
    {
        s_bFirstTime = false;
    }

    // Disable vsync
    pDeviceSettings->d3d11.SyncInterval = 0;

    // Don't auto create a depth buffer, as this sample requires a depth buffer
    // be created such that it's bindable as a shader resource
    pDeviceSettings->d3d11.AutoCreateDepthStencil = false;

    return true;
}

//-------------------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//-------------------------------------------------------------------------------------------------
void CALLBACK OnFrameMove(double time, float elapsedTime, void* pUserContext)
{
    g_pCurrentCamera->FrameMove(elapsedTime);
}

//-------------------------------------------------------------------------------------------------
// Handle messages to the application
//-------------------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
    void* pUserContext)
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc(hWnd, uMsg, wParam, lParam);
    if (*pbNoFurtherProcessing)
        return 0;

    // Pass messages to settings dialog if its active
    if (g_SettingsDlg.IsActive())
    {
        g_SettingsDlg.MsgProc(hWnd, uMsg, wParam, lParam);
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.m_GUI.MsgProc(hWnd, uMsg, wParam, lParam);
    if (*pbNoFurtherProcessing)
        return 0;

    // Pass all remaining windows messages to camera so it can respond to user input
    g_pCurrentCamera->HandleMessages(hWnd, uMsg, wParam, lParam);

    // Pass all remaining windows messages to the light Object
    g_LightCamera.HandleMessages(hWnd, uMsg, wParam, lParam);

    return 0;
}

//-------------------------------------------------------------------------------------------------
// Handle key presses
//-------------------------------------------------------------------------------------------------
void CALLBACK OnKeyboard(UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext)
{
    if (bKeyDown)
    {
        switch (nChar)
        {
        case VK_F1:
        g_RenderHUD = !g_RenderHUD;
        break;
        }
    }
}

//-------------------------------------------------------------------------------------------------
// Handles the GUI events
//-------------------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext)
{
    switch (nControlID)
    {
    case IDC_TOGGLEFULLSCREEN:
    DXUTToggleFullScreen();
    break;
    case IDC_TOGGLEREF:
    DXUTToggleREF();
    break;
    case IDC_CHANGEDEVICE:
    g_SettingsDlg.SetActive(!g_SettingsDlg.IsActive());
    break;

    case IDC_COMBOBOX_AO_LAYER_TECH:
    g_aoDesc.m_LayerProcess[g_aoLayer] = (AMD::AOFX_LAYER_PROCESS) g_HUD.m_GUI.GetComboBox(IDC_COMBOBOX_AO_LAYER_TECH)->GetSelectedIndex();

    if (g_aoDesc.m_LayerProcess[g_aoLayer] == AMD::AOFX_LAYER_PROCESS_COUNT)
        g_aoDesc.m_LayerProcess[g_aoLayer] = AMD::AOFX_LAYER_PROCESS_NONE;

    UpdateResolutionSlider(nControlID, pControl);
    break;

    case IDC_SLIDER_AO_BILATERAL_RADIUS:
    g_LayerBilateralBlurRadius.ui->OnGuiEvent();
    g_aoDesc.m_BilateralBlurRadius[g_aoLayer] = (AMD::AOFX_BILATERAL_BLUR_RADIUS) (g_LayerBilateralBlurRadius.ivalue - 1);
    break;

    case IDC_SLIDER_AO_POW_INTENSITY:
    g_LayerPowIntensity.ui->OnGuiEvent();
    g_aoDesc.m_PowIntensity[g_aoLayer] = (float)g_LayerPowIntensity.ivalue;
    break;

    case IDC_SLIDER_AO_LINEAR_INTENSITY:
    g_LayerLinearIntensity.ui->OnGuiEvent();
    g_aoDesc.m_LinearIntensity[g_aoLayer] = g_LayerLinearIntensity.fvalue;
    break;

    case IDC_SLIDER_AO_ACCEPT_RADIUS:
    g_LayerAcceptRadius.ui->OnGuiEvent();
    g_aoDesc.m_AcceptRadius[g_aoLayer] = g_LayerAcceptRadius.fvalue;
    break;

    case IDC_SLIDER_AO_ACCEPT_ANGLE:
    g_LayerDiscardDistance.ui->OnGuiEvent();
    g_aoDesc.m_ViewDistanceDiscard[g_aoLayer] = g_LayerDiscardDistance.fvalue;
    g_aoDesc.m_ViewDistanceFade[g_aoLayer] = g_LayerDiscardDistance.fvalue * 0.9f;
    break;

    case IDC_SLIDER_AO_REJECT_RADIUS:
    g_LayerRejectRadius.ui->OnGuiEvent();
    g_aoDesc.m_RejectRadius[g_aoLayer] = g_LayerRejectRadius.fvalue;
    break;

    case IDC_SLIDER_AO_NORMAL_SCALE:
    g_LayerNormalScale.ui->OnGuiEvent();
    g_aoDesc.m_NormalScale[g_aoLayer] = g_LayerNormalScale.fvalue;
    break;

    case IDC_SLIDER_AO_FADE_OUT_DISTANCE:
    g_LayerFadeOutDistance.ui->OnGuiEvent();
    g_aoDesc.m_RecipFadeOutDist[g_aoLayer] = g_LayerFadeOutDistance.fvalue;
    break;

    case IDC_SLIDER_AO_UPSAMPLE_THRESHOLD:
    g_LayerUpsampleThreshold.ui->OnGuiEvent();
    g_aoDesc.m_DepthUpsampleThreshold[g_aoLayer] = g_LayerUpsampleThreshold.fvalue;
    break;

    case IDC_CHECKBOX_AO_ENABLE_NORMALS:
    g_aoDesc.m_NormalOption[g_aoLayer] = g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_AO_ENABLE_NORMALS)->GetChecked() ? AMD::AOFX_NORMAL_OPTION_READ_FROM_SRV : AMD::AOFX_NORMAL_OPTION_NONE;
    UpdateResolutionSlider(nControlID, pControl);
    break;

    case IDC_COMBOBOX_AOFX_SAMPLE_COUNT:
    g_aoDesc.m_SampleCount[g_aoLayer] = (AMD::AOFX_SAMPLE_COUNT)((CDXUTComboBox*)pControl)->GetSelectedIndex();
    break;

    case IDC_COMBOBOX_AO_CURRENT_LAYER:
    g_aoLayer = ((CDXUTComboBox*)pControl)->GetSelectedIndex();
    SetUIFromAO(g_aoLayer);
    break;

    case IDC_COMBOBOX_AO_RANDOM_TAPS:
    g_aoDesc.m_TapType[g_aoLayer] = (AMD::AOFX_TAP_TYPE)((CDXUTComboBox*)pControl)->GetSelectedIndex();
    break;

    case IDC_CHECKBOX_AO_KERNEL_IMPLEMENTATION_PS:
    case IDC_CHECKBOX_AO_UTILITY_IMPLEMENTATION_PS:
    g_aoDesc.m_Implementation = AMD::AOFX_IMPLEMENTATION_MASK_BLUR_PS;
    g_aoDesc.m_Implementation |= g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_AO_KERNEL_IMPLEMENTATION_PS)->GetChecked() ? AMD::AOFX_IMPLEMENTATION_MASK_KERNEL_PS : AMD::AOFX_IMPLEMENTATION_MASK_KERNEL_CS;
    g_aoDesc.m_Implementation |= g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_AO_UTILITY_IMPLEMENTATION_PS)->GetChecked() ? AMD::AOFX_IMPLEMENTATION_MASK_UTILITY_PS : AMD::AOFX_IMPLEMENTATION_MASK_UTILITY_CS;
    break;

    case IDC_SLIDER_AO_DEPTH_DOWNSCALE:
    g_LayerDepthScale.ui->OnGuiEvent();
    g_aoDesc.m_MultiResLayerScale[g_aoLayer] = g_LayerDepthScale.fvalue;
    UpdateResolutionSlider(nControlID, pControl);
    break;

    default:
    break;
    }

    // Call the MagnifyTool gui event handler
    g_MagnifyTool.OnGUIEvent(nEvent, nControlID, pControl, pUserContext);
}

HRESULT UpdateResolutionSlider(const int kSliderIndex, CDXUTControl* pControl)
{
    g_aoDesc.m_InputSize.x = DXUTGetDXGIBackBufferSurfaceDesc()->Width;
    g_aoDesc.m_InputSize.y = DXUTGetDXGIBackBufferSurfaceDesc()->Height;
    g_aoDesc.m_Camera.m_NearPlane = g_ViewerCamera.GetNearClip();
    g_aoDesc.m_Camera.m_FarPlane = g_ViewerCamera.GetFarClip();

    AMD::AOFX_Resize(g_aoDesc);

    return S_OK;
}

void CreateShaders(ID3D11Device * pDevice)
{
    ID3DBlob *code_blob = NULL;

    // Ensure all shaders (and input layouts) are released first
    SAFE_RELEASE(g_pSceneIL);
    SAFE_RELEASE(g_pSceneVS);
    SAFE_RELEASE(g_pScenePS);
    SAFE_RELEASE(g_pRenderSceneWithAOPS);

    SAFE_RELEASE(g_pFullscreenPassVS);
    SAFE_RELEASE(g_pFullscreenPassPS);

    // Define our scene vertex data layout
    const D3D11_INPUT_ELEMENT_DESC SceneLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    if (AMD::CompileShaderFromFile(L"..\\src\\Shaders\\AOFX_Sample.hlsl", "vsRenderScene", "vs_5_0", &code_blob, NULL) == S_OK)
    {
        pDevice->CreateVertexShader(code_blob->GetBufferPointer(), code_blob->GetBufferSize(), NULL, &g_pSceneVS);
        pDevice->CreateInputLayout(SceneLayout, AMD_ARRAY_SIZE(SceneLayout), code_blob->GetBufferPointer(), code_blob->GetBufferSize(), &g_pSceneIL);
        SAFE_RELEASE(code_blob);
    }

    // Main scene VS and PS
    if (AMD::CompileShaderFromFile(L"..\\src\\Shaders\\AOFX_Sample.hlsl", "psRenderScene", "ps_5_0", &code_blob, NULL) == S_OK)
    {
        pDevice->CreatePixelShader(code_blob->GetBufferPointer(), code_blob->GetBufferSize(), NULL, &g_pScenePS);
        SAFE_RELEASE(code_blob);
    }

    if (AMD::CompileShaderFromFile(L"..\\src\\Shaders\\AOFX_Sample.hlsl", "psRenderSceneWithAO", "ps_5_0", &code_blob, NULL) == S_OK)
    {
        pDevice->CreatePixelShader(code_blob->GetBufferPointer(), code_blob->GetBufferSize(), NULL, &g_pRenderSceneWithAOPS);
        SAFE_RELEASE(code_blob);
    }

    // Fullscreen VS and Pass Through PS
    AMD::CreateFullscreenPass(&g_pFullscreenPassVS, DXUTGetD3D11Device());
    AMD::CreateFullscreenPass(&g_pFullscreenPassPS, DXUTGetD3D11Device());
}

//-------------------------------------------------------------------------------------------------
// EOF.
//-------------------------------------------------------------------------------------------------
