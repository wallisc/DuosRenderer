#include "D3D11Renderer.h"
#include "D3D11Util.h"
#include "RendererException.h"
#include "D3D11Canvas.h"

#include "dxtk/inc/WICTextureLoader.h"

#include <atlconv.h>
#include <d3dcompiler.h>
#include <directxcolors.h>

#define M_PI 3.14

using namespace DirectX;

const bool g_bGammaCorrectTextures = true;

enum VectorType
{
    DIRECTION,
    POSITION
};

XMVECTOR RealArrayToXMVector(Vec3 Vector, VectorType Type)
{
    return XMVectorSet(Vector.x, Vector.y, Vector.z, Type == DIRECTION ? (REAL)0.0 : (REAL)1.0);
}

Vec3 XMVectorToRealArray(XMVECTOR Vector)
{
    return Vec3(XMVectorGetX(Vector), XMVectorGetY(Vector), XMVectorGetZ(Vector));
}

D3D11_TEXTURECUBE_FACE ConvertRendererFaceToD3D11TextureFace(TextureFace Face)
{
    switch (Face)
    {
    case POS_X:
        return D3D11_TEXTURECUBE_FACE_POSITIVE_X;
    case POS_Y:
        return D3D11_TEXTURECUBE_FACE_POSITIVE_Y;
    case POS_Z:
        return D3D11_TEXTURECUBE_FACE_POSITIVE_Z;
    case NEG_X:
        return D3D11_TEXTURECUBE_FACE_NEGATIVE_X;
    case NEG_Y:
        return D3D11_TEXTURECUBE_FACE_NEGATIVE_Y;
    case NEG_Z:
        return D3D11_TEXTURECUBE_FACE_NEGATIVE_Z;
    }
}

//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DCompile
//
// With VS 11, we could load up prebuilt .cso files instead...
//--------------------------------------------------------------------------------------
HRESULT CompileShaderHelper(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, const D3D_SHADER_MACRO *pDefines, ID3DBlob** ppBlobOut)
{
    HRESULT hr = S_OK;

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;

    // Disable optimizations to further improve shader debugging
    dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* pErrorBlob = nullptr;
    hr = D3DCompileFromFile(szFileName, pDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, szEntryPoint, szShaderModel,
        dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
    if (FAILED(hr))
    {
        if (pErrorBlob)
        {
            OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
            pErrorBlob->Release();
        }
        return hr;
    }
    if (pErrorBlob) pErrorBlob->Release();

    return S_OK;
}

static void GenerateScreenSpaceVectorPlane(
    _In_ XMVECTOR TopLeftVector,
    _In_ XMVECTOR TopRightVector,
    _In_ XMVECTOR BottomLeftVector,
    _In_ XMVECTOR BottomRightVector,
    _Out_ CameraPlaneVertex *pVertexBufferData)
{
    {
        pVertexBufferData[0].m_ViewVector = XMVectorToRealArray(TopLeftVector);
        pVertexBufferData[0].m_Position = Vec3(-1.0, 1.0, 0.0);
    }

    {
        pVertexBufferData[1].m_ViewVector = XMVectorToRealArray(TopRightVector);
        pVertexBufferData[1].m_Position = Vec3(1.0, 1.0, 0.0);
    }
    {
        pVertexBufferData[2].m_ViewVector = XMVectorToRealArray(BottomLeftVector);
        pVertexBufferData[2].m_Position = Vec3(-1.0, -1.0, 0.0);
    }

    pVertexBufferData[3] = pVertexBufferData[2];
    pVertexBufferData[4] = pVertexBufferData[1];

    {
        pVertexBufferData[5].m_ViewVector = XMVectorToRealArray(BottomRightVector);
        pVertexBufferData[5].m_Position = Vec3(1.0, -1.0, 0.0);
    }
}


ReadWriteTexture::ReadWriteTexture(ID3D11Device *pDevice, UINT Width, UINT Height, DXGI_FORMAT Format)
{
    D3D11_TEXTURE2D_DESC ResourceDesc;
    ZeroMemory(&ResourceDesc, sizeof(ResourceDesc));
    ResourceDesc.Width = Width;
    ResourceDesc.Height = Height;
    ResourceDesc.MipLevels = 1;
    ResourceDesc.ArraySize = 1;
    ResourceDesc.Format = Format;
    ResourceDesc.SampleDesc.Count = 1;
    ResourceDesc.SampleDesc.Quality = 0;
    ResourceDesc.Usage = D3D11_USAGE_DEFAULT;
    ResourceDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    ResourceDesc.CPUAccessFlags = 0;
    ResourceDesc.MiscFlags = 0;
    HRESULT hr = pDevice->CreateTexture2D(&ResourceDesc, nullptr, &m_pResource);
    FAIL_CHK(FAILED(hr), "Failed creating a ReadWriteTexture");

    hr = pDevice->CreateRenderTargetView(m_pResource, nullptr, &m_pRenderTargetView);
    FAIL_CHK(FAILED(hr), "Failed to create RTV for ReadWriteTexture");

    hr = pDevice->CreateShaderResourceView(m_pResource, nullptr, &m_pShaderResourceView);
    FAIL_CHK(FAILED(hr), "Failed to create SRV for ReadWriteTexture");
}


D3D11Renderer::D3D11Renderer(HWND WindowHandle, unsigned int width, unsigned int height)
{
    UINT CreateDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef DEBUG
    CreateDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL FeatureLevels = D3D_FEATURE_LEVEL_11_1;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, CreateDeviceFlags, &FeatureLevels, 1,
        D3D11_SDK_VERSION, &m_pDevice, nullptr, &m_pImmediateContext);
    FAIL_CHK(FAILED(hr), "Failed D3D11CreateDevice");

#ifdef DEBUG
    CComPtr<ID3D11Debug> d3dDebug;
    if (SUCCEEDED(m_pDevice->QueryInterface(&d3dDebug)))
    {
        CComPtr<ID3D11InfoQueue> d3dInfoQueue;
        if (SUCCEEDED(d3dDebug->QueryInterface(&d3dInfoQueue)))
        {
            D3D11_MESSAGE_ID hide[] =
            {
                D3D11_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET,
                // Add more message IDs here as needed 
            };
            D3D11_INFO_QUEUE_FILTER filter;
            memset(&filter, 0, sizeof(filter));
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            d3dInfoQueue->AddStorageFilterEntries(&filter);
        }
    }
#endif

    hr = m_pDevice->QueryInterface(__uuidof(ID3D11Device1), (void**)&m_pDevice1);
    FAIL_CHK(FAILED(hr), "Failed query for ID3D11Device1");

    m_pBasePassTexture = std::unique_ptr<ReadWriteTexture>(new ReadWriteTexture(m_pDevice, width, height, DXGI_FORMAT_R8G8B8A8_UNORM));


    InitializeSwapchain(WindowHandle, width, height);

    CompileShaders();

    m_pBRDFPrecalculatedResources = std::unique_ptr<BRDFPrecomputationResouces>(new BRDFPrecomputationResouces(this));

    SetDefaultState();
}

void D3D11Renderer::SetCanvas(Canvas* pCanvas)
{
    m_pCanvas = nullptr;
    pCanvas->GetInterface(D3D11CanvasKey, (void **)&m_pCanvas);

    FAIL_CHK(m_pCanvas == nullptr, "Non-D3D11 canvas passed in"); // TODO: Need to add support for generic canvas
    ID3D11Resource *pCanvasResource;  
    HRESULT hr = m_pDevice1->OpenSharedResource1(m_pCanvas->GetCanvasResourceHandle(), __uuidof(ID3D11Resource), (void**)&pCanvasResource);
    FAIL_CHK(FAILED(hr), "Failed to open shared resource.");

    hr = m_pDevice->CreateRenderTargetView(pCanvasResource, nullptr, &m_pSwapchainRenderTargetView);
    FAIL_CHK(FAILED(hr), "Failed to create RTV for shared resource");
}

void D3D11Renderer::CompileShaders()
{
    // Compile the forward rendering shaders
    {
        CComPtr<ID3DBlob> pVSBlob = nullptr;
        HRESULT hr = CompileShaderHelper(L"GeometryRender.fx", "VS", "vs_4_0", nullptr, &pVSBlob);
        FAIL_CHK(FAILED(hr), "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.");
            
        hr = m_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_pForwardVertexShader);
        FAIL_CHK(FAILED(hr), "Failed to CreateVertexShader");

        // Define the input layout
        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        UINT numElements = ARRAYSIZE(layout);

        hr = m_pDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
            pVSBlob->GetBufferSize(), &m_pForwardInputLayout);

        pVSBlob = nullptr;
        hr = CompileShaderHelper(L"FullscreenPlane_VS.hlsl", "VS", "vs_5_0", nullptr, &pVSBlob);
        FAIL_CHK(FAILED(hr), "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.");

        hr = m_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_pFullscreenVS);
        FAIL_CHK(FAILED(hr), "Failed to CreateVertexShader");

        D3D_SHADER_MACRO macros[3] = {};
        macros[0].Name = "USE_TEXTURE";
        macros[0].Definition = "1";
        macros[1].Name = "USE_NORMAL_MAP";
        macros[1].Definition = "0";

        CComPtr<ID3DBlob> pPSBlob = nullptr;
        hr = CompileShaderHelper(L"GeometryRender.fx", "PS", "ps_4_0", macros, &pPSBlob);
        FAIL_CHK(FAILED(hr), "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.");

        hr = m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pForwardTexturedPixelShader);
        FAIL_CHK(FAILED(hr), "Failed to CreatePixelShader");

        ZeroMemory(macros, sizeof(macros));
        macros[0].Name = "USE_TEXTURE";
        macros[0].Definition = "1";
        macros[1].Name = "USE_NORMAL_MAP";
        macros[1].Definition = "1";
        pPSBlob = nullptr;

        hr = CompileShaderHelper(L"GeometryRender.fx", "PS", "ps_4_0", macros, &pPSBlob);
        FAIL_CHK(FAILED(hr), "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.");

        hr = m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pForwardTexturedBumpPixelShader);
        FAIL_CHK(FAILED(hr), "Failed to CreatePixelShader");

        ZeroMemory(macros, sizeof(macros));
        macros[0].Name = "USE_TEXTURE";
        macros[0].Definition = "0";
        pPSBlob = nullptr;
        hr = CompileShaderHelper(L"GeometryRender.fx", "PS", "ps_4_0", macros, &pPSBlob);
        FAIL_CHK(FAILED(hr), "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.");

        hr = m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pForwardMattePixelShader);
        FAIL_CHK(FAILED(hr), "Failed to CreatePixelShader");

        pPSBlob = nullptr;
        hr = CompileShaderHelper(L"GammaCorrect_PS.hlsl", "PS", "ps_5_0", nullptr, &pPSBlob);
        FAIL_CHK(FAILED(hr), "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.");

        hr = m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pGammaCorrectPS);
        FAIL_CHK(FAILED(hr), "Failed to CreatePixelShader");

        pPSBlob = nullptr;
        hr = CompileShaderHelper(L"PassThrough_PS.hlsl", "PS", "ps_5_0", nullptr, &pPSBlob);
        FAIL_CHK(FAILED(hr), "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.");

        hr = m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pPassThroughPS);
        FAIL_CHK(FAILED(hr), "Failed to CreatePixelShader");
    }
}

void D3D11Renderer::InitializeSwapchain(HWND WindowHandle, unsigned int width, unsigned int height)
{
    assert(m_pDevice);

    ID3D11Texture2D *pDepthStencil;
    D3D11_TEXTURE2D_DESC descDepth;
    ZeroMemory(&descDepth, sizeof(descDepth));
    descDepth.Width = width;
    descDepth.Height = height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    descDepth.CPUAccessFlags = 0;
    descDepth.MiscFlags = 0;
    HRESULT hr = m_pDevice->CreateTexture2D(&descDepth, nullptr, &pDepthStencil);
    FAIL_CHK(FAILED(hr), "Failed creating a depth stencil resource");

    // Create the depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
    ZeroMemory(&descDSV, sizeof(descDSV));
    descDSV.Format = descDepth.Format;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;
    hr = m_pDevice->CreateDepthStencilView(pDepthStencil, &descDSV, &m_pDepthBuffer);
    FAIL_CHK(FAILED(hr), "Failed creating a depth stencil view");

    // Setup the viewport
    m_viewport.Width = (FLOAT)width;
    m_viewport.Height = (FLOAT)height;
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;
    m_viewport.TopLeftX = 0;
    m_viewport.TopLeftY = 0;

    ID3D11Texture2D *pShadowBuffer;
    D3D11_TEXTURE2D_DESC ShadowBufferDesc;
    ZeroMemory(&ShadowBufferDesc, sizeof(ShadowBufferDesc));
    ShadowBufferDesc.Width = width;
    ShadowBufferDesc.Height = height;
    ShadowBufferDesc.MipLevels = 1;
    ShadowBufferDesc.ArraySize = 1;
    ShadowBufferDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    ShadowBufferDesc.SampleDesc.Count = 1;
    ShadowBufferDesc.SampleDesc.Quality = 0;
    ShadowBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    ShadowBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    ShadowBufferDesc.CPUAccessFlags = 0;
    ShadowBufferDesc.MiscFlags = 0;
    hr = m_pDevice->CreateTexture2D(&ShadowBufferDesc, nullptr, &pShadowBuffer);
    FAIL_CHK(FAILED(hr), "Failed creating a shadow depth stencil resource");

    // Create the depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC ShadowDepthViewDesc;
    ZeroMemory(&ShadowDepthViewDesc, sizeof(ShadowDepthViewDesc));
    ShadowDepthViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    ShadowDepthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    ShadowDepthViewDesc.Texture2D.MipSlice = 0;
    hr = m_pDevice->CreateDepthStencilView(pShadowBuffer, &ShadowDepthViewDesc, &m_pShadowDepthBuffer);
    FAIL_CHK(FAILED(hr), "Failed creating a shadow depth stencil view");

    D3D11_SHADER_RESOURCE_VIEW_DESC ShadowResourceViewDesc;
    ZeroMemory(&ShadowResourceViewDesc, sizeof(ShadowResourceViewDesc));
    ShadowResourceViewDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    ShadowResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    ShadowResourceViewDesc.Texture2D.MipLevels = 1;
    ShadowResourceViewDesc.Texture2D.MostDetailedMip = 0;
    hr = m_pDevice->CreateShaderResourceView(pShadowBuffer, &ShadowResourceViewDesc, &m_pShadowResourceView);
    FAIL_CHK(FAILED(hr), "Failed creating a shadow srv");
}

void D3D11Renderer::SetDefaultState()
{
    HRESULT hr = S_OK;
    D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.StencilEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;

    CComPtr<ID3D11DepthStencilState> pDepthStencilState;
    hr = m_pDevice->CreateDepthStencilState(&depthStencilDesc, &pDepthStencilState);
    FAIL_CHK(FAILED(hr), "Failed CreateDepthStencilState");

    m_pImmediateContext->OMSetDepthStencilState(pDepthStencilState, 0);

    // Create the sample state
    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory(&sampDesc, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerState);
    FAIL_CHK(FAILED(hr), "Failed creating the sampler state");

    m_pImmediateContext->PSSetSamplers(0, 1, &m_pSamplerState);
    m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

Geometry* D3D11Renderer::CreateGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor)
{
    D3D11Geometry* pGeometry = new D3D11Geometry(m_pDevice, m_pImmediateContext, pCreateGeometryDescriptor);
    MEM_CHK(pGeometry);
    return pGeometry;
}

void D3D11Renderer::DestroyGeometry(_In_ Geometry *pGeometry)
{
    delete pGeometry;
}

Light *D3D11Renderer::CreateLight(_In_ CreateLightDescriptor *pCreateLightDescriptor)
{
    Light *pLight = nullptr;
    switch (pCreateLightDescriptor->m_LightType)
    {
    case CreateLightDescriptor::DIRECTIONAL_LIGHT:
        pLight = new D3D11DirectionalLight(m_pDevice, pCreateLightDescriptor);
        break;
    default:
        assert(false);
        break;
    }

    MEM_CHK(pLight);
    return pLight;
}

void D3D11Renderer::DestroyLight(Light *pLight)
{
    delete pLight;
}

Camera *D3D11Renderer::CreateCamera(_In_ CreateCameraDescriptor *pCreateCameraDescriptor)
{
    Camera *pCamera = new D3D11Camera(m_pDevice, m_pImmediateContext, pCreateCameraDescriptor);
    MEM_CHK(pCamera);
    return pCamera;
}

void D3D11Renderer::DestroyCamera(Camera *pCamera)
{
    delete pCamera;
}

D3D11Material::D3D11Material(_In_ ID3D11Device *pDevice, ID3D11DeviceContext *pContext, CreateMaterialDescriptor *pCreateMaterialDescriptor) :
    m_pContext(pContext),
    m_pDiffuseTexture(nullptr),
    m_pNormalMap(nullptr)
{
    HRESULT result = S_OK;
    if (pCreateMaterialDescriptor->m_TextureName && strlen(pCreateMaterialDescriptor->m_TextureName))
    {
        CA2WEX<MAX_ALLOWED_STR_LENGTH> WideTextureName(pCreateMaterialDescriptor->m_TextureName);
        result = CreateWICTextureFromFileEx(
            pDevice, 
            WideTextureName, 
            20 * 1024 * 1024, 
            D3D11_USAGE_DEFAULT, 
            D3D11_BIND_SHADER_RESOURCE, 
            0, 
            0, 
            g_bGammaCorrectTextures,
            nullptr, 
            &m_pDiffuseTexture);
        FAIL_CHK(FAILED(result), "Failed to create SRV for texture");
    }

    if (pCreateMaterialDescriptor->m_NormalMapName && strlen(pCreateMaterialDescriptor->m_NormalMapName))
    {
        CA2WEX<MAX_ALLOWED_STR_LENGTH> WideTextureName(pCreateMaterialDescriptor->m_NormalMapName);
        result = CreateWICTextureFromFile(pDevice,
            WideTextureName,
            nullptr,
            &m_pNormalMap,
            20 * 1024 * 1024);
        FAIL_CHK(FAILED(result), "Failed to create SRV for texture");
    }
    auto &diffuse = pCreateMaterialDescriptor->m_DiffuseColor;
    m_CBMaterial.m_Diffuse = XMVectorSet(diffuse.x, diffuse.y, diffuse.z, 1.0f);
    m_CBMaterial.m_MaterialProperties = XMVectorSet(pCreateMaterialDescriptor->m_Reflectivity, pCreateMaterialDescriptor->m_Roughness, 0, 0);

    D3D11_BUFFER_DESC MaterialBufferDesc;
    ZeroMemory(&MaterialBufferDesc, sizeof(MaterialBufferDesc));
    MaterialBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    MaterialBufferDesc.ByteWidth = sizeof(CBMaterial);
    MaterialBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    MaterialBufferDesc.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA MaterialBufferData;
    ZeroMemory(&MaterialBufferData, sizeof(MaterialBufferData));
    MaterialBufferData.pSysMem = &m_CBMaterial;
    HRESULT hr = pDevice->CreateBuffer(&MaterialBufferDesc, &MaterialBufferData, &m_pMaterialBuffer);
    FAIL_CHK(FAILED(hr), "Failed to create Index Buffer");
}

void D3D11Material::SetRoughness(float Roughness)
{
    UpdateMaterialProperty(Roughness, CBMaterial::ROUGHNESS_INDEX);
}

void D3D11Material::SetReflectivity(float Reflectivity)
{
    UpdateMaterialProperty(Reflectivity, CBMaterial::REFLECTIVITY_INDEX);
}

void D3D11Material::UpdateMaterialProperty(float value, unsigned int index)
{
    m_CBMaterial.m_MaterialProperties = XMVectorSetByIndex(m_CBMaterial.m_MaterialProperties, value, index);
    UpdateMaterialBuffer();
}


void D3D11Material::UpdateMaterialBuffer()
{
    m_pContext->UpdateSubresource(m_pMaterialBuffer, 0, nullptr, &m_CBMaterial, 0, 0);
}

Material *D3D11Renderer::CreateMaterial(_In_ CreateMaterialDescriptor *pCreateMaterialDescriptor)
{
    D3D11Material *pMaterial = new D3D11Material(m_pDevice, m_pImmediateContext, pCreateMaterialDescriptor);
    MEM_CHK(pMaterial);
    return pMaterial;
}

void D3D11Renderer::DestroyMaterial(Material* pMaterial)
{
    delete pMaterial;
}

D3D11EnvironmentTextureCube::D3D11EnvironmentTextureCube(
    _In_ D3D11Renderer *pRenderer,
    _In_ const CreateEnvironmentTextureCube *pCreateTextureCube) :
    D3D11EnvironmentMap(pRenderer)
{
    auto pDevice = GetParent()->GetD3D11Device();
    auto pImmediateContext = GetParent()->GetD3D11Context();
    ID3D11ShaderResourceView *pCubeMap = CreateTextureCube(pDevice, pImmediateContext, pCreateTextureCube->m_TextureNames);

    D3D11_BUFFER_DESC CameraVertexBufferDesc;
    ZeroMemory(&CameraVertexBufferDesc, sizeof(CameraVertexBufferDesc));
    CameraVertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    CameraVertexBufferDesc.ByteWidth = sizeof(CameraPlaneVertex) * VerticesInCameraPlane;
    CameraVertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    CameraVertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HRESULT hr = pDevice->CreateBuffer(&CameraVertexBufferDesc, nullptr, &m_pCameraVertexBuffer);

    {
        ID3D10Blob *m_pPixelShaderBlob;
        CompileShaderHelper(L"EnvironmentShader.fx", "PS", "ps_5_0", nullptr, &m_pPixelShaderBlob);
        hr = pDevice->CreatePixelShader(m_pPixelShaderBlob->GetBufferPointer(), m_pPixelShaderBlob->GetBufferSize(), nullptr, &m_pEnvironmentPixelShader);
        FAIL_CHK(FAILED(hr), "Failed to compile pixel shader");

        ID3D10Blob *m_pVertexShaderBlob;
        CompileShaderHelper(L"EnvironmentShader.fx", "VS", "vs_5_0", nullptr, &m_pVertexShaderBlob);
        hr = pDevice->CreateVertexShader(m_pVertexShaderBlob->GetBufferPointer(), m_pVertexShaderBlob->GetBufferSize(), nullptr, &m_pEnvironmentVertexShader);
        FAIL_CHK(FAILED(hr), "Failed to compile vertex shader");


        D3D11_INPUT_ELEMENT_DESC inputElements[] = 
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };

        hr = pDevice->CreateInputLayout(inputElements, ARRAYSIZE(inputElements), m_pVertexShaderBlob->GetBufferPointer(), m_pVertexShaderBlob->GetBufferSize(), &m_pEnvironmentInputLayout);
        FAIL_CHK(FAILED(hr), "Failed to create the input layout for the environment shader");
    }

    m_pEnvironmentTextureCube = pCubeMap;
    m_pPrefilteredTextureCube[0] = m_pEnvironmentTextureCube;
    for (UINT i = 1; i < cNumberOfPrefilteredCubes; i++)
    {
        m_pPrefilteredTextureCube[i] = GenerateBakedReflectionCube(pCubeMap, (float)i / (float)(cNumberOfPrefilteredCubes - 1));
    }
}

ID3D11ShaderResourceView *D3D11EnvironmentTextureCube::GenerateBakedReflectionCube(ID3D11ShaderResourceView *pCubeMap, float roughness)
{
    auto pImmediateContext = GetParent()->GetD3D11Context();
    auto pDevice = GetParent()->GetD3D11Device();
    auto &BRDFResources = GetParent()->GetBRDFPrecomputedResources();
    pImmediateContext->PSSetShader(BRDFResources.GetPixelShader(), nullptr, 0);
    pImmediateContext->VSSetShader(BRDFResources.GetVertexShader(), nullptr, 0);
    pImmediateContext->IASetInputLayout(BRDFResources.GetInputLayout());

    CComPtr<ID3D11Resource> pInputResource;
    pCubeMap->GetResource(&pInputResource);

    ID3D11Texture2D* pOutputResource;
    const UINT cBoxHeight = 512;
    D3D11_TEXTURE2D_DESC cubeMapDesc = CD3D11_TEXTURE2D_DESC(
        DXGI_FORMAT_B8G8R8X8_UNORM,
        cBoxHeight, 
        cBoxHeight, 
        6, 
        1, 
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
        D3D11_USAGE_DEFAULT,
        0, 1, 0,
        D3D11_RESOURCE_MISC_TEXTURECUBE);
    pDevice->CreateTexture2D(&cubeMapDesc, nullptr, &pOutputResource);

    D3D11_SHADER_RESOURCE_VIEW_DESC cubeMapSRV = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURECUBE, cubeMapDesc.Format, 0, 1);
    ID3D11ShaderResourceView* pOutputSRV;
    pDevice->CreateShaderResourceView(pOutputResource, &cubeMapSRV, &pOutputSRV);

    ID3D11ShaderResourceView *pSRVs[] = { pCubeMap };
    pImmediateContext->PSSetShaderResources(0, 1, pSRVs);

    D3D11_VIEWPORT viewport = CD3D11_VIEWPORT(0.0f, 0.0f, cBoxHeight, cBoxHeight);
    pImmediateContext->RSSetViewports(1, &viewport);

    // TODO: Set rasterizer/blend state

    for (UINT face = 0; face < NUM_CUBE_FACES; face++)
    {
        ID3D11Buffer *pBuffers[] = { BRDFResources.GetCubeMapVertexBuffer((CUBE_FACES)face) };
        UINT pStride[] = { BRDFResources.GetCubeMapVertexBufferStride() };
        UINT pOffset[] = { 0 };
    
        CComPtr<ID3D11RenderTargetView> pRenderTarget;
        D3D11_RENDER_TARGET_VIEW_DESC renderTargetDesc = CD3D11_RENDER_TARGET_VIEW_DESC(pOutputResource, D3D11_RTV_DIMENSION_TEXTURE2DARRAY, cubeMapDesc.Format, 0, face, 1);
        pDevice->CreateRenderTargetView(pOutputResource, &renderTargetDesc, &pRenderTarget);
        
        ID3D11RenderTargetView *pRTVs[] = { pRenderTarget };
        float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        pImmediateContext->ClearRenderTargetView(pRenderTarget, black);
    
        ID3D11Buffer *pMaterialBuffer = BRDFResources.GetMaterialBuffer(roughness);
    
        ID3D11Buffer* pConstantBuffers[] = { pMaterialBuffer };
    
        pImmediateContext->OMSetRenderTargets(1, pRTVs, nullptr);
        pImmediateContext->IASetVertexBuffers(0, ARRAYSIZE(pBuffers), pBuffers, pStride, pOffset);
        pImmediateContext->PSSetConstantBuffers(0, ARRAYSIZE(pConstantBuffers), pConstantBuffers);
        pImmediateContext->Draw(D3D11GeometryHelper::cScreenSpacePlaneVertexCount, 0);
    }

    return pOutputSRV;
}

ID3D11ShaderResourceView *D3D11EnvironmentTextureCube::CreateTextureCube(
    _In_ ID3D11Device *pDevice, 
    _In_ ID3D11DeviceContext *pImmediateContext,
    _In_reads_(TEXTURES_PER_CUBE) char * const *textureNames)
{
    ID3D11Texture2D *pCubeResource;
    ID3D11ShaderResourceView *pShaderResourceView;
    for (UINT i = 0; i < TEXTURES_PER_CUBE; i++)
    {
        CA2WEX<MAX_ALLOWED_STR_LENGTH> WideTextureName(textureNames[i]);
        ID3D11Resource *pTempResource;

        HRESULT hr = CreateWICTextureFromFileEx(
            pDevice,
            WideTextureName,
            20 * 1024 * 1024,
            D3D11_USAGE_DEFAULT,
            D3D11_BIND_SHADER_RESOURCE,
            0,
            0,
            g_bGammaCorrectTextures,
            &pTempResource,
            nullptr);

        FAIL_CHK(FAILED(hr), "Failed to create texture from cube texture name");

        if (i == 0)
        {
            D3D11_TEXTURE2D_DESC TextureDesc;
            ID3D11Texture2D *pTempTexture;
            pTempResource->QueryInterface<ID3D11Texture2D>(&pTempTexture);
            pTempTexture->GetDesc(&TextureDesc);

            D3D11_TEXTURE2D_DESC TextureCubeDesc = CD3D11_TEXTURE2D_DESC(
                TextureDesc.Format, TextureDesc.Width, TextureDesc.Height, TEXTURES_PER_CUBE, 1,
                D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, 1, 0, D3D11_RESOURCE_MISC_TEXTURECUBE);

            hr = pDevice->CreateTexture2D(&TextureCubeDesc, nullptr, &pCubeResource);
            FAIL_CHK(FAILED(hr), "Failed to create texture cube");

            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURECUBE, TextureCubeDesc.Format);
            hr = pDevice->CreateShaderResourceView(pCubeResource, &srvDesc, &pShaderResourceView);
            FAIL_CHK(FAILED(hr), "Failed to create SRV for texture cube");
        }

        pImmediateContext->CopySubresourceRegion(pCubeResource, ConvertRendererFaceToD3D11TextureFace((TextureFace)i), 0, 0, 0, pTempResource, 0, nullptr);
    }
    return pShaderResourceView;
}

void D3D11EnvironmentTextureCube::DrawEnvironmentMap(_In_ ID3D11DeviceContext *pImmediateContext, D3D11Camera *pCamera, ID3D11RenderTargetView *pRenderTarget)
{
    D3D11_MAPPED_SUBRESOURCE MappedSubresource;
    HRESULT hr = pImmediateContext->Map(m_pCameraVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedSubresource);
    FAIL_CHK(FAILED(hr), "Failed to map camera vertex buffer for environment map");

    CameraPlaneVertex *pVertexBuffer = (CameraPlaneVertex *)MappedSubresource.pData;

    const XMVECTOR viewRight = pCamera->GetRight();
    XMVECTOR viewUp = pCamera->GetUp();
    XMVECTOR LensPosition = pCamera->GetPosition();
    XMVECTOR LensCenterPosition = XMVectorAdd(pCamera->GetPosition(), XMVectorScale(pCamera->GetViewDirection(), pCamera->GetFocalLength()));
    XMVECTOR viewDirection = pCamera->GetViewDirection();
    float horizontalFOV = pCamera->GetHorizontalFieldOfView();
    float verticalFOV = pCamera->GetVerticalFieldOfView();

    XMVECTOR TopLeftLensCorner = XMVectorAdd(LensCenterPosition, XMVectorScale(pCamera->GetRight(), pCamera->GetLensWidth() / 2.0f));
    TopLeftLensCorner = XMVectorAdd(TopLeftLensCorner, XMVectorScale(pCamera->GetUp(), pCamera->GetLensHeight() / 2.0f));
    XMVECTOR TopLeftViewVector = XMVector3Normalize(XMVectorSubtract(TopLeftLensCorner, pCamera->GetPosition()));

    XMVECTOR TopRightLensCorner = XMVectorAdd(LensCenterPosition, XMVectorScale(pCamera->GetRight(), -pCamera->GetLensWidth() / 2.0f));
    TopRightLensCorner = XMVectorAdd(TopRightLensCorner, XMVectorScale(pCamera->GetUp(), pCamera->GetLensHeight() / 2.0f));
    XMVECTOR TopRightViewVector = XMVector3Normalize(XMVectorSubtract(TopRightLensCorner, pCamera->GetPosition()));
     
    XMVECTOR BottomRLeftLensCorner = XMVectorAdd(LensCenterPosition, XMVectorScale(pCamera->GetRight(), pCamera->GetLensWidth() / 2.0f));
    BottomRLeftLensCorner = XMVectorAdd(BottomRLeftLensCorner, XMVectorScale(pCamera->GetUp(), -pCamera->GetLensHeight() / 2.0f));
    XMVECTOR BottomLeftViewVector = XMVector3Normalize(XMVectorSubtract(BottomRLeftLensCorner, pCamera->GetPosition()));

    XMVECTOR BottomRightLensCorner = XMVectorAdd(LensCenterPosition, XMVectorScale(pCamera->GetRight(), -pCamera->GetLensWidth() / 2.0f));
    BottomRightLensCorner = XMVectorAdd(BottomRightLensCorner, XMVectorScale(pCamera->GetUp(), -pCamera->GetLensHeight() / 2.0f));
    XMVECTOR BottomRightViewVector = XMVector3Normalize(XMVectorSubtract(BottomRightLensCorner, pCamera->GetPosition()));

    GenerateScreenSpaceVectorPlane(TopLeftViewVector, TopRightViewVector, BottomLeftViewVector, BottomRightViewVector, pVertexBuffer);

    pImmediateContext->Unmap(m_pCameraVertexBuffer, 0);

    const UINT stride = sizeof(CameraPlaneVertex);
    const UINT offset = 0;
    pImmediateContext->IASetVertexBuffers(0, 1, &m_pCameraVertexBuffer, &stride, &offset);
    pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pImmediateContext->IASetInputLayout(m_pEnvironmentInputLayout);
    pImmediateContext->PSSetShader(m_pEnvironmentPixelShader, nullptr, 0);
    pImmediateContext->VSSetShader(m_pEnvironmentVertexShader, nullptr, 0);
    pImmediateContext->PSSetShaderResources(0, 1, &m_pEnvironmentTextureCube);
    pImmediateContext->OMSetRenderTargets(1, &pRenderTarget, nullptr);

    pImmediateContext->DrawInstanced(VerticesInCameraPlane, 1, 0, 0);
}

EnvironmentMap *D3D11Renderer::CreateEnvironmentMap(CreateEnvironmentMapDescriptor *pCreateEnvironmnetMapDescriptor)
{
    EnvironmentMap *pEnvironmentMap;
    switch (pCreateEnvironmnetMapDescriptor->m_EnvironmentType)
    {
    case CreateEnvironmentMapDescriptor::SOLID_COLOR:
        pEnvironmentMap = new D3D11EnvironmentColor(this, &pCreateEnvironmnetMapDescriptor->m_SolidColor);
        break;
    case CreateEnvironmentMapDescriptor::TEXTURE_CUBE:
        pEnvironmentMap = new D3D11EnvironmentTextureCube(this, &pCreateEnvironmnetMapDescriptor->m_TextureCube);
        break;
    }

    return pEnvironmentMap;
}

void D3D11Renderer::DestroyEnviromentMap(EnvironmentMap *pEnvironmentMap)
{
    delete pEnvironmentMap;
}

Scene *D3D11Renderer::CreateScene(EnvironmentMap *pEnvironmentMap)
{
    D3D11EnvironmentMap *pD3D11EnvironmentMap = (D3D11EnvironmentMap *)pEnvironmentMap;
    FAIL_CHK(pEnvironmentMap == nullptr, "Null environment passed into CreateScene");

    Scene *pScene = new D3D11Scene(pD3D11EnvironmentMap);
    MEM_CHK(pScene);
    return pScene;
}

void D3D11Renderer::DestroyScene(Scene* pScene)
{
    delete pScene;
}

class D3D11Pass
{
public:
    virtual void SetBindings(ID3D11DeviceContext *pContext) = 0;
    virtual void UndoBindings(ID3D11DeviceContext *pContext) = 0;
};

class D3D11ShadowPass : public D3D11Pass
{
public:
    D3D11ShadowPass(ID3D11Buffer *pViewProjBuffer, ID3D11DepthStencilView *pDepthBuffer) :
        m_pDepthBuffer(pDepthBuffer), m_pViewProjBuffer(pViewProjBuffer) {}

    void SetBindings(ID3D11DeviceContext *pContext)
    {
        pContext->VSSetConstantBuffers(1, 1, &m_pViewProjBuffer);
        pContext->OMSetRenderTargets(0, nullptr, m_pDepthBuffer);
    }

    virtual void UndoBindings(ID3D11DeviceContext *pContext)
    {
        pContext->OMSetRenderTargets(0, nullptr, nullptr);
    }

private:
    ID3D11Buffer *m_pViewProjBuffer;
    ID3D11DepthStencilView* m_pDepthBuffer;
};

class D3D11BasePass : public D3D11Pass
{
public:
    D3D11BasePass(
        ID3D11Buffer *pViewProjBuffer, 
        ID3D11Buffer *pLightViewProjBuffer, 
        ID3D11ShaderResourceView *pShadowView,
        ID3D11ShaderResourceView *pBRDF_LUT,
        ID3D11RenderTargetView *pRenderTarget, 
        ID3D11DepthStencilView *pDepthBuffer) :
        m_pDepthBuffer(pDepthBuffer), 
        m_pViewProjBuffer(pViewProjBuffer), 
        m_pLightViewProjBuffer(pLightViewProjBuffer),
        m_pBRDF_LUT(pBRDF_LUT),
        m_pRenderTarget(pRenderTarget),
        m_pShadowView(pShadowView) {}

    void SetBindings(ID3D11DeviceContext *pContext)
    {
        pContext->OMSetRenderTargets(1, &m_pRenderTarget, m_pDepthBuffer);
        pContext->VSSetConstantBuffers(1, 1, &m_pViewProjBuffer);
        pContext->PSSetConstantBuffers(1, 1, &m_pViewProjBuffer);
        pContext->VSSetConstantBuffers(3, 1, &m_pLightViewProjBuffer);
        pContext->PSSetShaderResources(1, 1, &m_pShadowView);
        pContext->PSSetShaderResources(5, 1, &m_pBRDF_LUT);

    }

    virtual void UndoBindings(ID3D11DeviceContext *pContext)
    {
        ID3D11ShaderResourceView *pNullSRV = nullptr;
        pContext->OMSetRenderTargets(0, nullptr, nullptr);
        pContext->PSSetShaderResources(1, 1, &pNullSRV);
        pContext->PSSetShaderResources(2, 1, &pNullSRV);
        pContext->PSSetShaderResources(3, 1, &pNullSRV);
    }

private:
    ID3D11Buffer *m_pViewProjBuffer;
    ID3D11Buffer *m_pLightViewProjBuffer;
    ID3D11RenderTargetView* m_pRenderTarget;
    ID3D11DepthStencilView* m_pDepthBuffer;
    ID3D11ShaderResourceView *m_pShadowView;
    ID3D11ShaderResourceView *m_pBRDF_LUT;
    ID3D11ShaderResourceView *m_pIrradianceMap;
};

void D3D11Renderer::DrawScene(Camera *pCamera, Scene *pScene, const RenderSettings &RenderFlags)
{
    D3D11Scene *pD3D11Scene = D3D11_RENDERER_CAST<D3D11Scene*>(pScene);
    D3D11Camera *pD3D11Camera = D3D11_RENDERER_CAST<D3D11Camera*>(pCamera);

    for (auto pLight : pD3D11Scene->GetDirectionalLightList())
    {
        pLight->UpdateLight(m_pImmediateContext, pD3D11Camera->GetInvTransViewMatrix());
    }

    pD3D11Scene->GetEnvironmentMap()->DrawEnvironmentMap(m_pImmediateContext, pD3D11Camera, m_pBasePassTexture->GetRenderTargetView());
    m_pImmediateContext->ClearDepthStencilView(m_pDepthBuffer, D3D11_CLEAR_DEPTH, 1.0f, 0);
    m_pImmediateContext->ClearDepthStencilView(m_pShadowDepthBuffer, D3D11_CLEAR_DEPTH, 1.0f, 0);
    m_pImmediateContext->RSSetViewports(1, &m_viewport);


    ID3D11Buffer *pCameraConstantBuffer = pD3D11Camera->GetCameraConstantBuffer();
    m_pImmediateContext->IASetInputLayout(m_pForwardInputLayout);

    assert(pD3D11Scene->GetDirectionalLightList().size() == 1); // TODO hard coded for one directional light
    ID3D11Buffer *pDirectonalLightConstantBuffer = pD3D11Scene->GetDirectionalLightList()[0]->GetDirectionalLightConstantBuffer();
    m_pImmediateContext->PSSetConstantBuffers(2, 1, &pDirectonalLightConstantBuffer);
    m_pImmediateContext->VSSetConstantBuffers(0, 1, &pCameraConstantBuffer);

    m_pImmediateContext->VSSetShader(m_pForwardVertexShader, NULL, 0);

    const unsigned int Strides = sizeof(Vertex);
    const unsigned int Offsets = 0;

    ID3D11Buffer *pCameraViewProjBuffer = pD3D11Camera->GetViewProjBuffer();
    ID3D11Buffer *pLightViewProjBuffer = pD3D11Scene->GetDirectionalLightList()[0]->GetViewProjBuffer(m_pImmediateContext, pD3D11Scene, pD3D11Camera);

    std::vector<D3D11Pass *> Passes;
    D3D11ShadowPass ShadowPass(pLightViewProjBuffer, m_pShadowDepthBuffer);
    Passes.push_back(&ShadowPass);
    D3D11BasePass BasePass(
        pCameraViewProjBuffer, 
        pLightViewProjBuffer, 
        m_pShadowResourceView, 
        GetBRDFPrecomputedResources().GetBRDF_LUT(),
        m_pBasePassTexture->GetRenderTargetView(), 
        m_pDepthBuffer);
    Passes.push_back(&BasePass);

    for (D3D11Pass *pPass : Passes)
    {
        pPass->SetBindings(m_pImmediateContext);

        for (auto pGeometry : pD3D11Scene->m_GeometryList)
        {
            ID3D11Buffer *pVertexBuffer = pGeometry->GetVertexBuffer();
            ID3D11Buffer *pIndexBuffer = pGeometry->GetIndexBuffer();
            ID3D11Buffer *pMaterialBuffer = pGeometry->GetD3D11Material()->GetMaterialBuffer();

            m_pImmediateContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &Strides, &Offsets);
            m_pImmediateContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
            m_pImmediateContext->PSSetConstantBuffers(4, 1, &pMaterialBuffer);

            const float roughness = pGeometry->GetMaterial()->GetRoughness();
            ID3D11ShaderResourceView *pEnvironmentMapLowerBound = pD3D11Scene->GetEnvironmentMap()->GetLowerBoundEnvironmentMapSRV(roughness);
            ID3D11ShaderResourceView *pEnvironmentMapUpperBound = pD3D11Scene->GetEnvironmentMap()->GetUpperBoundEnvironmentMapSRV(roughness);

            m_pImmediateContext->PSSetShaderResources(2, 1, &pEnvironmentMapLowerBound);
            m_pImmediateContext->PSSetShaderResources(3, 1, &pEnvironmentMapUpperBound);

            if (pGeometry->GetD3D11Material()->HasDiffuseTexture())
            {
                ID3D11ShaderResourceView *pDiffuseTexture = pGeometry->GetD3D11Material()->GetDiffuseTexture();
                m_pImmediateContext->PSSetShaderResources(0, 1, &pDiffuseTexture);
                if (!pGeometry->GetD3D11Material()->HasNormalMap())
                {
                    m_pImmediateContext->PSSetShader(m_pForwardTexturedPixelShader, NULL, 0);
                    m_pImmediateContext->PSSetShaderResources(0, 1, &pDiffuseTexture);
                }
                else
                {
                    ID3D11ShaderResourceView *pNormalMap = pGeometry->GetD3D11Material()->GetNormalMap();
                    m_pImmediateContext->PSSetShaderResources(4, 1, &pNormalMap);
                    m_pImmediateContext->PSSetShader(m_pForwardTexturedBumpPixelShader, NULL, 0);
                }
            }
            else
            {
                m_pImmediateContext->PSSetShader(m_pForwardMattePixelShader, NULL, 0);
            }

            if (pIndexBuffer)
            {
                m_pImmediateContext->DrawIndexed(pGeometry->GetIndexCount(), 0, 0);
            }
            else
            {
                m_pImmediateContext->Draw(pGeometry->GetVertexCount(), 0);
            }
        }
        pPass->UndoBindings(m_pImmediateContext);
    }
    

    ID3D11ShaderResourceView *NullViews[4] = {};
    m_pImmediateContext->PSSetShaderResources(0, ARRAYSIZE(NullViews), NullViews);

    PostProcess(m_pBasePassTexture.get(), m_pSwapchainRenderTargetView, RenderFlags);
    m_pImmediateContext->Flush();
}

class PostProcessPass : public D3D11Pass
{
public:
    PostProcessPass(ID3D11PixelShader *pPixelShader, ID3D11VertexShader *pVertexShader,
        UINT NumInputs, _In_reads_(NumInputs) ID3D11ShaderResourceView **pInputViews,
        UINT NumOutputs, _In_reads_(NumOutputs) ID3D11RenderTargetView **pOutputViews) :
        m_pPixelShader(pPixelShader), m_pVertexShader(pVertexShader)
    {
        for (UINT i = 0; i < NumInputs; i++)
        {
            m_pInputs.push_back(pInputViews[i]);
        }

        for (UINT i = 0; i < NumOutputs; i++)
        {
            m_pOutputs.push_back(pOutputViews[i]);
        }
    }

    void SetBindings(ID3D11DeviceContext *pContext)
    {
        pContext->PSSetShader(m_pPixelShader, nullptr, 0);
        pContext->VSSetShader(m_pVertexShader, nullptr, 0);

        pContext->PSSetShaderResources(0, m_pInputs.size(), &m_pInputs[0]);
        pContext->OMSetRenderTargets(m_pOutputs.size(), &m_pOutputs[0], nullptr);
    }

    void UndoBindings(ID3D11DeviceContext *pContext)
    {
        static ID3D11ShaderResourceView *pNullSRVs[128] = {};
        pContext->PSSetShaderResources(0, m_pInputs.size(), pNullSRVs);
        pContext->OMSetRenderTargets(0, nullptr, nullptr);
    }

private:
    ID3D11PixelShader *m_pPixelShader;
    ID3D11VertexShader *m_pVertexShader;
    std::vector<ID3D11ShaderResourceView*> m_pInputs;
    std::vector<ID3D11RenderTargetView*> m_pOutputs;
};

void D3D11Renderer::PostProcess(ReadWriteTexture *pInput, ID3D11RenderTargetView *pOutput, const RenderSettings &RenderFlags)
{
    std::vector<D3D11Pass *> PostProcessPasses;

    ID3D11ShaderResourceView *pSRVs = pInput->GetShaderResourceView();
    PostProcessPass GammaCorrectionPass(m_pGammaCorrectPS, m_pFullscreenVS, 1, &pSRVs, 1, &pOutput);
    PostProcessPass PassThroughPass(m_pPassThroughPS, m_pFullscreenVS, 1, &pSRVs, 1, &pOutput);
    if (RenderFlags.m_GammaCorrection)
    {
        PostProcessPasses.push_back(&GammaCorrectionPass);
    }
    else
    {
        PostProcessPasses.push_back(&PassThroughPass);
    }

    m_pImmediateContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    m_pImmediateContext->IASetInputLayout(nullptr);
    for (auto pPass : PostProcessPasses)
    {
        pPass->SetBindings(m_pImmediateContext);
        m_pImmediateContext->Draw(3, 0);
        pPass->UndoBindings(m_pImmediateContext);
    }

}

D3D11Transformable::D3D11Transformable(_In_ ID3D11Device *pDevice, ID3D11DeviceContext *pImmediateContext) :
    m_pImmediateContext(pImmediateContext)
{
    m_WorldMatrix = XMMatrixIdentity();

    D3D11_BUFFER_DESC WorldMatrixDesc;
    ZeroMemory(&WorldMatrixDesc, sizeof(WorldMatrixDesc));
    WorldMatrixDesc.Usage = D3D11_USAGE_DEFAULT;
    WorldMatrixDesc.ByteWidth = sizeof(DirectX::XMMATRIX);
    WorldMatrixDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    WorldMatrixDesc.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA WorldMatrixData;
    ZeroMemory(&WorldMatrixData, sizeof(WorldMatrixData));
    WorldMatrixData.pSysMem = &m_WorldMatrix;
    HRESULT hr = pDevice->CreateBuffer(&WorldMatrixDesc, &WorldMatrixData, &m_pWorldTransform);
    FAIL_CHK(FAILED(hr), "Failed to create Index Buffer");
}

ID3D11Buffer *D3D11Transformable::GetWorldTransformBuffer()
{
    m_pImmediateContext->UpdateSubresource(m_pWorldTransform, 0, nullptr, &m_WorldMatrix, 0, 0);
    return m_pWorldTransform;
}

void D3D11Transformable::Translate(_In_ const Vec3 &TranslationVector)
{
    m_WorldMatrix *= XMMatrixTranslation(TranslationVector.x, TranslationVector.y, TranslationVector.z);
}

D3D11Geometry::D3D11Geometry(_In_ ID3D11Device *pDevice, _In_ ID3D11DeviceContext *pImmediateContext, _In_ CreateGeometryDescriptor *pCreateGeometryDescriptor) :
    D3D11Transformable(pDevice, pImmediateContext), m_pIndexBuffer(0), m_pVertexBuffer(0)
{
    HRESULT hr = S_OK;
    m_UsesIndexBuffer = pCreateGeometryDescriptor->m_NumIndices != 0;
    m_VertexCount = pCreateGeometryDescriptor->m_NumVertices;
    m_IndexCount = pCreateGeometryDescriptor->m_NumIndices;
    m_pMaterial = D3D11_RENDERER_CAST<D3D11Material *>(pCreateGeometryDescriptor->m_pMaterial);
    if (m_UsesIndexBuffer)
    {
        D3D11_BUFFER_DESC IndexBufferDesc;
        ZeroMemory(&IndexBufferDesc, sizeof(IndexBufferDesc));
        IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
        IndexBufferDesc.ByteWidth = sizeof(unsigned int)* pCreateGeometryDescriptor->m_NumIndices;
        IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        IndexBufferDesc.CPUAccessFlags = 0;
        D3D11_SUBRESOURCE_DATA IndexBufferData;
        ZeroMemory(&IndexBufferData, sizeof(IndexBufferData));
        IndexBufferData.pSysMem = pCreateGeometryDescriptor->m_pIndices;
        hr = pDevice->CreateBuffer(&IndexBufferDesc, &IndexBufferData, &m_pIndexBuffer);
        FAIL_CHK(FAILED(hr), "Failed to create Index Buffer");
    }

    D3D11_BUFFER_DESC VertexBufferDesc;
    ZeroMemory(&VertexBufferDesc, sizeof(VertexBufferDesc));
    VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    VertexBufferDesc.ByteWidth = sizeof(Vertex)* pCreateGeometryDescriptor->m_NumVertices;
    VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    VertexBufferDesc.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA VertexBufferData;
    ZeroMemory(&VertexBufferData, sizeof(VertexBufferData));
    VertexBufferData.pSysMem = pCreateGeometryDescriptor->m_pVertices;
    hr = pDevice->CreateBuffer(&VertexBufferDesc, &VertexBufferData, &m_pVertexBuffer);
    FAIL_CHK(FAILED(hr), "Failed to create Index Buffer");

    float MaxX, MaxY, MaxZ, MinX, MinY, MinZ;
    MaxX = MaxY = MaxZ = -FLT_MAX;
    MinX = MinY = MinZ = FLT_MAX;
    for (int i = 0; i < pCreateGeometryDescriptor->m_NumVertices; i++)
    {
        const Vertex *pVertex = &pCreateGeometryDescriptor->m_pVertices[i];
        
        MaxX = max(MaxX, pVertex->m_Position.x);
        MaxY = max(MaxY, pVertex->m_Position.y);
        MaxZ = max(MaxZ, pVertex->m_Position.z);

        MinX = min(MinX, pVertex->m_Position.x);
        MinY = min(MinY, pVertex->m_Position.y);
        MinZ = min(MinZ, pVertex->m_Position.z);
    }

    m_MaxDimensions = XMVectorSet(MaxX, MaxY, MaxZ, 0.0f);
    m_MinDimensions = XMVectorSet(MinX, MinY, MinZ, 0.0f);
}

D3D11Geometry::~D3D11Geometry()
{
    m_pVertexBuffer->Release();
    if (m_pIndexBuffer) { m_pIndexBuffer->Release(); };
}

D3D11DirectionalLight::D3D11DirectionalLight(
    _In_ ID3D11Device *pDevice, 
    _In_ CreateLightDescriptor *pCreateLight) :
    D3D11Light(CreateLightDescriptor::DIRECTIONAL_LIGHT)
{
    assert(pCreateLight->m_LightType == CreateLightDescriptor::DIRECTIONAL_LIGHT);

    m_lightDirection = -1.0f * XMVector3Normalize(RealArrayToXMVector(
        pCreateLight->m_pCreateDirectionalLight->m_EmissionDirection, DIRECTION));

    // Flip direction since we want light direction, not emission direction
    m_CpuLightData.m_Direction = m_lightDirection;
    m_CpuLightData.m_Color = RealArrayToXMVector(pCreateLight->m_Color, DIRECTION);

    m_pLightConstantBuffer = CreateConstantBuffer(pDevice, &m_CpuLightData, sizeof(CBDirectionalLight));
    
    // We need scene information to know where we should render the shadow map but we can at least 
    // allocate the buffer now
    m_ViewProjCpuData.m_Projection = XMMatrixIdentity();
    m_ViewProjCpuData.m_View = XMMatrixIdentity();
    m_ViewProjCpuData.m_InvTransView = XMMatrixIdentity();
    m_pViewProjBuffer = CreateConstantBuffer(pDevice, &m_ViewProjCpuData, sizeof(CBViewProjectionTransforms));
}

void D3D11DirectionalLight::UpdateLight(ID3D11DeviceContext *pContext, const XMMATRIX &invTransView)
{
    m_CpuLightData.m_Direction = XMVector3Normalize(XMVector4Transform(m_lightDirection, invTransView));
    pContext->UpdateSubresource(m_pLightConstantBuffer, 0, nullptr, &m_CpuLightData, 0, 0);
}

ID3D11Buffer *D3D11DirectionalLight::GetViewProjBuffer(ID3D11DeviceContext *pContext, D3D11Scene *pScene, D3D11Camera *pCamera)
{
    // Calculate what's the biggest the scene can actually be.
    float MaxSceneLength = XMVectorGetX(XMVector3Length(pScene->GetMaxDimensions() - pScene->GetMinDimensions()));
    XMVECTOR SceneCenter = pScene->GetMaxDimensions() + pScene->GetMinDimensions();
    XMVECTOR EyePosition = SceneCenter + m_lightDirection * MaxSceneLength;

    XMVECTOR LookDir = XMVector3Normalize(SceneCenter - EyePosition);
    XMVECTOR Up;
    if (XMVectorGetX(LookDir) > .99f)
    {
        Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    }
    else if (XMVectorGetY(LookDir) > .99f)
    {
        Up = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    }
    else
    {
        // Calculate the Up vector by assuming x = 0
        float LookDirY = XMVectorGetY(LookDir);
        float LookDirZ = XMVectorGetZ(LookDir);
        float z = 1.0f / ((LookDirZ * LookDirZ) / (LookDirY * LookDirY) + 1);
        float y = -z * LookDirZ / LookDirY;

        Up = XMVectorSet(0.0f, y, z, 0.0f);
    }

    XMVECTOR determinant;
    m_ViewProjCpuData.m_View = XMMatrixLookAtLH(EyePosition, SceneCenter, Up);
    m_ViewProjCpuData.m_InvTransView = XMMatrixTranspose(XMMatrixInverse(&determinant, m_ViewProjCpuData.m_View));
    m_ViewProjCpuData.m_Projection = XMMatrixOrthographicLH(
        MaxSceneLength,
        MaxSceneLength / pCamera->GetAspectRatio(),
        0.0001f,
        MaxSceneLength * 2.0f);

    pContext->UpdateSubresource(m_pViewProjBuffer, 0, nullptr, &m_ViewProjCpuData, 0, 0);
    return m_pViewProjBuffer;
}


D3D11Scene::D3D11Scene(D3D11EnvironmentMap *pEnvironmentMap) :
    m_pEnvironmentMap(pEnvironmentMap)
{
    m_MaxDimensions = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.0f);
    m_MinDimensions = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.0f);
}

void D3D11Scene::AddGeometry(_In_ Geometry *pGeometry)
{
    D3D11Geometry *pD3D11Geometry = (D3D11Geometry*)(pGeometry);
    
    XMVECTOR GeometryMaxDimensions = pD3D11Geometry->GetMaxDimensions();
    XMVECTOR GeometryMinDimensions = pD3D11Geometry->GetMinDimensions();
    for (int i = 0; i < 3; i++)
    {
        m_MaxDimensions = XMVectorSetByIndex(
            m_MaxDimensions,
            max(XMVectorGetByIndex(m_MaxDimensions, i), XMVectorGetByIndex(GeometryMaxDimensions, i)),
            i);

        m_MinDimensions = XMVectorSetByIndex(
            m_MinDimensions,
            min(XMVectorGetByIndex(m_MinDimensions, i), XMVectorGetByIndex(GeometryMinDimensions, i)),
            i);
    }

    m_GeometryList.push_back(pD3D11Geometry);
}

void D3D11Scene::AddLight(_In_ Light *pLight)
{
    D3D11Light *pD3D11Light = (D3D11Light*)(pLight);
    switch (pD3D11Light->GetLightType())
    {
    case CreateLightDescriptor::DIRECTIONAL_LIGHT:
        assert(m_DirectionalLightList.size() == 0); // TODO: Only supporting 1 directional light for now
        m_DirectionalLightList.push_back((D3D11DirectionalLight*)(pLight));
        break;
    default:
        assert(false);
        break;
    }
}

D3D11Camera::D3D11Camera(ID3D11Device *pDevice, ID3D11DeviceContext *pContext, CreateCameraDescriptor *pCreateCameraDescriptor) :
    m_pContext(pContext), 
    m_VerticalFieldOfView(pCreateCameraDescriptor->m_VerticalFieldOfView),
    m_Width(pCreateCameraDescriptor->m_Width),
    m_Height(pCreateCameraDescriptor->m_Height)
{
    const float LensHeight = 2.0f;
    const float AspectRatio = pCreateCameraDescriptor->m_Width / pCreateCameraDescriptor->m_Height;
    const float LensWidth = 2.0f * AspectRatio;

    m_HorizontalFieldOfView = 2 * atan(LensWidth / (2.0f * GetFocalLength()));
    m_Position = RealArrayToXMVector(pCreateCameraDescriptor->m_FocalPoint, POSITION);
    m_LookAt = RealArrayToXMVector(pCreateCameraDescriptor->m_LookAt, POSITION);
    m_Up = RealArrayToXMVector(pCreateCameraDescriptor->m_Up, DIRECTION);

    m_CameraCpuData.m_ClipDistance = XMVectorSet(pCreateCameraDescriptor->m_FarClip, 0, 0, 0);
    m_CameraCpuData.m_Dimensions = XMVectorSet(pCreateCameraDescriptor->m_Width, pCreateCameraDescriptor->m_Height, 0, 0);
    
    XMVECTOR determinant;
    m_ViewProjCpuData.m_Projection = XMMatrixPerspectiveFovLH(pCreateCameraDescriptor->m_VerticalFieldOfView,
        pCreateCameraDescriptor->m_Width / pCreateCameraDescriptor->m_Height,
        pCreateCameraDescriptor->m_NearClip,
        pCreateCameraDescriptor->m_FarClip);
    m_ViewProjCpuData.m_View = XMMatrixLookAtLH(m_Position, m_LookAt, m_Up);
    m_ViewProjCpuData.m_InvTransView = XMMatrixTranspose(XMMatrixInverse(&determinant, m_ViewProjCpuData.m_View));
    m_ViewProjCpuData.m_CamPos = m_Position;

    m_pCameraBuffer = CreateConstantBuffer(pDevice, &m_CameraCpuData, sizeof(CBCamera));
    m_pViewProjBuffer = CreateConstantBuffer(pDevice, &m_ViewProjCpuData, sizeof(CBViewProjectionTransforms));
}

D3D11Camera::~D3D11Camera()
{

}

void D3D11Camera::Translate(_In_ const Vec3 &translationVector)
{
    XMVECTOR lookDir = XMVector3Normalize(m_LookAt - m_Position);
    XMVECTOR right = XMVector3Cross(lookDir, m_Up);
    XMVECTOR delta = translationVector.x * right + translationVector.y * m_Up + translationVector.z * lookDir;
    m_Position += delta;
    m_LookAt += delta;
    m_ViewMatrixNeedsUpdate = true;
}

void D3D11Camera::Rotate(float row, float yaw, float pitch)
{
    XMVECTOR lookDir = GetViewDirection();

    if (pitch)
    {
        XMVECTOR right = GetRight();
        XMMATRIX pitchRotation = XMMatrixRotationNormal(right, pitch);
        lookDir = XMVector3Transform(lookDir, pitchRotation);
        m_Up = XMVector3Normalize(XMVector3Transform(m_Up, pitchRotation));
    }

    if (yaw)
    {
        XMMATRIX yawRotation = XMMatrixRotationNormal(XMVectorSet(0, 1, 0, 0), yaw);
        lookDir = XMVector3Normalize(XMVector3Transform(lookDir, yawRotation));
        m_Up = XMVector3Normalize(XMVector3Transform(m_Up, yawRotation));
    }

    m_LookAt = m_Position + XMVector3Length(m_LookAt - m_Position) * lookDir;

    m_ViewMatrixNeedsUpdate = true;
}

ID3D11Buffer* D3D11Camera::GetCameraConstantBuffer()
{
    return m_pCameraBuffer;
}

ID3D11Buffer* D3D11Camera::GetViewProjBuffer()
{
    if (m_ViewMatrixNeedsUpdate)
    {
        XMVECTOR determinant;
        m_ViewProjCpuData.m_View = XMMatrixLookAtLH(m_Position, m_LookAt, m_Up);
        m_ViewProjCpuData.m_InvTransView = XMMatrixTranspose(XMMatrixInverse(&determinant, m_ViewProjCpuData.m_View));
        m_ViewProjCpuData.m_CamPos = m_Position;

        m_pContext->UpdateSubresource(m_pViewProjBuffer, 0, nullptr, &m_ViewProjCpuData, 0, 0);
        m_ViewMatrixNeedsUpdate = false;
    }
    return m_pViewProjBuffer;
}

float FloatRand()
{
    return rand() / (float)RAND_MAX;
}

BRDFPrecomputationResouces::BRDFPrecomputationResouces(D3D11Renderer *pRenderer) :
    D3D11RendererChild(pRenderer)
{
    auto pDevice = pRenderer->GetD3D11Device();
    auto pImmediateContext = pRenderer->GetD3D11Context();
    HRESULT hr = S_OK;

    CComPtr<ID3DBlob> pPSBlob = nullptr;
    CompileShaderHelper(L"PrefilterCube_PS.hlsl", "main", "ps_5_0", nullptr, &pPSBlob);

    hr = pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pPrecalcBRDFPixelShader);
    FAIL_CHK(FAILED(hr), "Failed Creating PrefilterCube Pixel Shader");

    CComPtr<ID3DBlob> pVSBlob = nullptr;
    CompileShaderHelper(L"PrefilterCube_VS.hlsl", "main", "vs_5_0", nullptr, &pVSBlob);
    hr = pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_pPrecalcBRDFVertexShader);
    FAIL_CHK(FAILED(hr), "Failed Creating PrefilterCube Vertex Shader");

    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };  
    UINT numElements = ARRAYSIZE(layout);
    hr = pDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pPrecalcBRDFInputLayout);

    const D3D11_BUFFER_DESC materialBufferDesc = CD3D11_BUFFER_DESC(sizeof(CBPrecalcBRDFMaterial), D3D11_BIND_CONSTANT_BUFFER);
    pDevice->CreateBuffer(&materialBufferDesc, nullptr, &m_pMaterialBuffer);

    const D3D11_BUFFER_DESC vertexBufferDesc = CD3D11_BUFFER_DESC(sizeof(CameraPlaneVertex) * 6, D3D11_BIND_VERTEX_BUFFER);
    D3D11_SUBRESOURCE_DATA initialData;
    CameraPlaneVertex pVertexBufferData[D3D11GeometryHelper::cScreenSpacePlaneVertexCount];

    initialData.pSysMem = pVertexBufferData;
    for (UINT face = 0; face < NUM_CUBE_FACES; face++)
    {
        XMVECTOR TopLeftViewVector;
        XMVECTOR TopRightViewVector;
        XMVECTOR BottomLeftViewVector;
        XMVECTOR BottomRightViewVector;
        switch (face)
        {
        case CUBE_FACES::X_POSITIVE:
            TopLeftViewVector = XMVector3Normalize(XMVectorSet(1.0, 1.0, 1.0, 0.0f));
            TopRightViewVector = XMVector3Normalize(XMVectorSet(1.0, 1.0, -1.0, 0.0f));
            BottomLeftViewVector = XMVector3Normalize(XMVectorSet(1.0, -1.0, 1.0, 0.0f));
            BottomRightViewVector = XMVector3Normalize(XMVectorSet(1.0, -1.0, -1.0, 0.0f));
            break;
        case CUBE_FACES::Y_POSITIVE:
            TopLeftViewVector = XMVector3Normalize(XMVectorSet(1.0, 1.0, -1.0, 0.0f));
            TopRightViewVector = XMVector3Normalize(XMVectorSet(-1.0, 1.0, -1.0, 0.0f));
            BottomLeftViewVector = XMVector3Normalize(XMVectorSet(1.0, 1.0, 1.0, 0.0f));
            BottomRightViewVector = XMVector3Normalize(XMVectorSet(-1.0, 1.0, 1.0, 0.0f));
            break;
        case CUBE_FACES::Z_POSITIVE:
            TopLeftViewVector = XMVector3Normalize(XMVectorSet(1.0, 1.0, 1.0, 0.0f));
            TopRightViewVector = XMVector3Normalize(XMVectorSet(-1.0, 1.0, 1.0, 0.0f));
            BottomLeftViewVector = XMVector3Normalize(XMVectorSet(1.0, -1.0, 1.0, 0.0f));
            BottomRightViewVector = XMVector3Normalize(XMVectorSet(-1.0, -1.0, 1.0, 0.0f));
            break;
        case CUBE_FACES::X_NEGATIVE:
            TopLeftViewVector = XMVector3Normalize(XMVectorSet(-1.0, 1.0, -1.0, 0.0f));
            TopRightViewVector = XMVector3Normalize(XMVectorSet(-1.0, 1.0, 1.0, 0.0f));
            BottomLeftViewVector = XMVector3Normalize(XMVectorSet(-1.0, -1.0, -1.0, 0.0f));
            BottomRightViewVector = XMVector3Normalize(XMVectorSet(-1.0, -1.0, 1.0, 0.0f));
            break;
        case CUBE_FACES::Y_NEGATIVE:
            TopLeftViewVector = XMVector3Normalize(XMVectorSet(1.0, -1.0, -1.0, 0.0f));
            TopRightViewVector = XMVector3Normalize(XMVectorSet(-1.0, -1.0, -1.0, 0.0f));
            BottomLeftViewVector = XMVector3Normalize(XMVectorSet(1.0, -1.0, 1.0, 0.0f));
            BottomRightViewVector = XMVector3Normalize(XMVectorSet(-1.0, -1.0, 1.0, 0.0f));
            break;
        case CUBE_FACES::Z_NEGATIVE:
            TopLeftViewVector = XMVector3Normalize(XMVectorSet(1.0, 1.0, -1.0, 0.0f));
            TopRightViewVector = XMVector3Normalize(XMVectorSet(-1.0, 1.0, -1.0, 0.0f));
            BottomLeftViewVector = XMVector3Normalize(XMVectorSet(1.0, -1.0, -1.0, 0.0f));
            BottomRightViewVector = XMVector3Normalize(XMVectorSet(-1.0, -1.0, -1.0, 0.0f));
            break;
        default:
            assert(false);
            return;
        }
        GenerateScreenSpaceVectorPlane(TopLeftViewVector, TopRightViewVector, BottomLeftViewVector, BottomRightViewVector, pVertexBufferData);
        pDevice->CreateBuffer(&vertexBufferDesc, &initialData, &m_pCubeMapBuffers[face]);
    }

    InitBRDFLUT();
}

void BRDFPrecomputationResouces::InitBRDFLUT()
{
    auto pDevice = GetParent()->GetD3D11Device();
    auto pImmediateContext = GetParent()->GetD3D11Context();

    const UINT integratedBRDFTextureWidth = 512;
    const UINT integratedBRDFTextureHeight = 512;
    ReadWriteTexture BRDFTex(pDevice, integratedBRDFTextureWidth, integratedBRDFTextureHeight, DXGI_FORMAT_R16G16_FLOAT);
    m_pIntegratedBRDF = BRDFTex.GetShaderResourceView();
    
    D3D11_VIEWPORT viewport = CD3D11_VIEWPORT(0.0f, 0.0f, (float)integratedBRDFTextureWidth, (float)integratedBRDFTextureHeight);

    CComPtr<ID3DBlob> pPSBlob = nullptr;
    CompileShaderHelper(L"PrecalcBRDF_PS.hlsl", "main", "ps_5_0", nullptr, &pPSBlob);
    CComPtr<ID3D11PixelShader> pPrecalcBRDFShader;
    pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &pPrecalcBRDFShader);

    ID3D11RenderTargetView *ppRTVs[] = { BRDFTex.GetRenderTargetView() };
    pImmediateContext->RSSetViewports(1, &viewport);
    pImmediateContext->OMSetRenderTargets(1, ppRTVs, nullptr);
    pImmediateContext->IASetInputLayout(nullptr);
    pImmediateContext->VSSetShader(GetParent()->GetFullscreenVS(), nullptr, 0);
    pImmediateContext->PSSetShader(pPrecalcBRDFShader, nullptr, 0);
    pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pImmediateContext->Draw(D3D11GeometryHelper::cScreenSpacePlaneVertexCount, 0);
}


ID3D11Buffer *BRDFPrecomputationResouces::GetMaterialBuffer(float roughness) const
{
    CBPrecalcBRDFMaterial MaterialConstant;
    MaterialConstant.roughness = XMVectorSet(roughness, 0, 0, 0);
    GetParent()->GetD3D11Context()->UpdateSubresource(m_pMaterialBuffer, 0, nullptr, &MaterialConstant, 0, 0);
    return m_pMaterialBuffer;
}
