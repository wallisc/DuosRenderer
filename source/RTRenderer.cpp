#include "RTRenderer.h"
#include "RendererException.h"

#include <d3dcompiler.h>
#include <atlbase.h>

using namespace DirectX;

enum VectorType
{
	DIRECTION,
	POSITION
};

XMVECTOR RealArrayToXMVector2(_In_reads_(3) Vec3 Vector, VectorType Type)
{
	return XMVectorSet(Vector.x, Vector.y, Vector.z, Type == DIRECTION ? (REAL)0.0 : (REAL)1.0);
}

//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DCompile
//
// With VS 11, we could load up prebuilt .cso files instead...
//--------------------------------------------------------------------------------------
HRESULT CompileShaderHelper2(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
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
	hr = D3DCompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel,
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

RTRenderer::RTRenderer(HWND WindowHandle, unsigned int width, unsigned int height)
{
	UINT CeateDeviceFlags = 0;
#ifdef DEBUG
	CeateDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	D3D_FEATURE_LEVEL FeatureLevels = D3D_FEATURE_LEVEL_11_1;
	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, CeateDeviceFlags, &FeatureLevels, 1,
		D3D11_SDK_VERSION, &m_pDevice, nullptr, &m_pImmediateContext);
	FAIL_CHK(FAILED(hr), "Failed D3D11CreateDevice");

	InitializeSwapchain(WindowHandle, width, height);

	CompileShaders();
	SetDefaultState();
}

void RTRenderer::CompileShaders()
{
	// Compile the forward rendering shaders
	{
		CComPtr<ID3DBlob> pVSBlob = nullptr;
		HRESULT hr = CompileShaderHelper2(L"Plane_VS.hlsl", "main", "vs_5_0", &pVSBlob);
		FAIL_CHK(FAILED(hr), "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.");

		hr = m_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_pForwardVertexShader);
		FAIL_CHK(FAILED(hr), "Failed to CreateVertexShader");

		CComPtr<ID3DBlob> pPSBlob = nullptr;
		hr = CompileShaderHelper2(L"Copy_PS.hlsl", "main", "ps_4_0", &pPSBlob);
		FAIL_CHK(FAILED(hr), "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.");

		hr = m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pForwardPixelShader);
		FAIL_CHK(FAILED(hr), "Failed to CreatePixelShader");
	}
}

void RTRenderer::InitializeSwapchain(HWND WindowHandle, unsigned int width, unsigned int height)
{
	assert(m_pDevice);
	HRESULT hr = S_OK;
	// Obtain DXGI factory from device (since we used nullptr for pAdapter above)
	IDXGIFactory1* dxgiFactory = nullptr;
	{
		IDXGIDevice* dxgiDevice = nullptr;
		FAIL_CHK(FAILED(hr), "Failed to query interface for DXGIDevice");
		hr = m_pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
		IDXGIAdapter* adapter = nullptr;
		hr = dxgiDevice->GetAdapter(&adapter);
		if (SUCCEEDED(hr))
		{
			hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
			adapter->Release();
		}
		dxgiDevice->Release();
	}

	// Create swap chain
	CComPtr<IDXGIFactory2> dxgiFactory2 = nullptr;
	hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
	FAIL_CHK(!dxgiFactory2, "DXGIFactory2 is null");
	DXGI_SWAP_CHAIN_DESC1 sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.Width = width;
	sd.Height = height;
	sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 1;

	hr = dxgiFactory2->CreateSwapChainForHwnd(m_pDevice, WindowHandle, &sd, nullptr, nullptr, &m_pSwapChain1);
	if (SUCCEEDED(hr))
	{
		hr = m_pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&m_pSwapChain));
	}

	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));

	hr = m_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pSwapchainRenderTargetView);
	pBackBuffer->Release();
	FAIL_CHK(FAILED(hr), "Failed to create render target view for back buffer");

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_pImmediateContext->RSSetViewports(1, &vp);
}

void RTRenderer::SetDefaultState()
{
	m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

Geometry* RTRenderer::CreateGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor)
{
	RTGeometry* pGeometry = new RTGeometry(m_pDevice, pCreateGeometryDescriptor);
	MEM_CHK(pGeometry);
	return pGeometry;
}

void RTRenderer::DestroyGeometry(_In_ Geometry *pGeometry)
{
	delete pGeometry;
}

Light *RTRenderer::CreateLight(_In_ CreateLightDescriptor *pCreateLightDescriptor)
{
	RTLight *pLight = new RTLight(pCreateLightDescriptor);
	MEM_CHK(pLight);
	return pLight;
}

void RTRenderer::DestroyLight(Light *pLight)
{
	delete pLight;
}

Camera *RTRenderer::CreateCamera(_In_ CreateCameraDescriptor *pCreateCameraDescriptor)
{
	Camera *pCamera = new RTCamera(m_pDevice, pCreateCameraDescriptor);
	MEM_CHK(pCamera);
	return pCamera;
}

void RTRenderer::DestroyCamera(Camera *pCamera)
{
	delete pCamera;
}

Scene *RTRenderer::CreateScene()
{
	Scene *pScene = new RTScene();
	MEM_CHK(pScene);
	return pScene;
}

void RTRenderer::DestroyScene(Scene* pScene)
{
	delete pScene;
}

void RTRenderer::DrawScene(Camera *pCamera, Scene *pScene)
{
	m_pImmediateContext->PSSetShader(m_pForwardPixelShader, NULL, 0);
	m_pImmediateContext->VSSetShader(m_pForwardVertexShader, NULL, 0);
	m_pImmediateContext->OMSetRenderTargets(1, &m_pSwapchainRenderTargetView, NULL);

	m_pImmediateContext->Draw(3, 0);

	m_pSwapChain->Present(0, 0);
}

RTGeometry::RTGeometry(_In_ ID3D11Device *pDevice, _In_ CreateGeometryDescriptor *pCreateGeometryDescriptor)
{
}

RTGeometry::~RTGeometry()
{
}

void RTGeometry::Update(_In_ Transform *pTransform)
{
	assert(false); // Implement
}

RTLight::RTLight(_In_ CreateLightDescriptor *pCreateLightDescriptor)
{
	switch (pCreateLightDescriptor->m_LightType)
	{
	default:
		assert(false);
		break;
	}
}

void RTScene::AddGeometry(_In_ Geometry *pGeometry)
{
	RTGeometry *pRTGeometry = D3D11_RENDERER_CAST<RTGeometry*>(pGeometry);
	m_GeometryList.push_back(pRTGeometry);
}

RTCamera::RTCamera(ID3D11Device *pDevice, CreateCameraDescriptor *pCreateCameraDescriptor)
{
}

RTCamera::~RTCamera()
{

}

void RTCamera::Update(_In_ Transform *pTransform)
{
	assert(false);
}