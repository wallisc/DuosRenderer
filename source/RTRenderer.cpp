#include "RTRenderer.h"
#include "RendererException.h"

#include <d3dcompiler.h>
#include <atlbase.h>

using namespace DirectX;

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
	hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&m_pBackBuffer));

	FAIL_CHK(FAILED(hr), "Failed to create render target view for back buffer");

	D3D11_TEXTURE2D_DESC MappableTextureDesc = {};
	MappableTextureDesc.ArraySize = 1;
	MappableTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	MappableTextureDesc.Height = height;
	MappableTextureDesc.Width = width;
	MappableTextureDesc.MipLevels = 1;
	MappableTextureDesc.Usage = D3D11_USAGE_DYNAMIC;
	MappableTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	MappableTextureDesc.SampleDesc.Count = 1;
	MappableTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	hr = m_pDevice->CreateTexture2D(&MappableTextureDesc, NULL, &m_pMappableTexture);
	FAIL_CHK(FAILED(hr), "Failed to create mappable resource");
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
	RTScene *pRTScene = RT_RENDERER_CAST<RTScene*>(pScene);
	RTCamera *pRTCamera = RT_RENDERER_CAST<RTCamera*>(pCamera);

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	m_pImmediateContext->Map(m_pMappableTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);

	UINT m_Width = 800;
	UINT m_Height = 600;
	void *pRow = MappedResource.pData;

	RTD3DMappedCanvas Canvas(MappedResource.pData, MappedResource.RowPitch, DXGI_FORMAT_R8G8B8A8_UNORM);
	RTracer::Trace(pRTScene, pRTCamera, &Canvas);
	m_pImmediateContext->Unmap(m_pMappableTexture, 0);

	m_pImmediateContext->CopyResource(m_pBackBuffer, m_pMappableTexture);
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
	RTGeometry *pRTGeometry = RT_RENDERER_CAST<RTGeometry*>(pGeometry);
	m_GeometryList.push_back(pRTGeometry);
}

RTCamera::RTCamera(ID3D11Device *pDevice, CreateCameraDescriptor *pCreateCameraDescriptor) :
	m_Width(pCreateCameraDescriptor->m_Width), m_Height(pCreateCameraDescriptor->m_Height) {}

RTCamera::~RTCamera()
{

}

UINT RTCamera::GetWidth() { return m_Width; }
UINT RTCamera::GetHeight() { return m_Height; }

void RTCamera::Update(_In_ Transform *pTransform)
{
	assert(false);
}

RTD3DMappedCanvas::RTD3DMappedCanvas(void *pMappedSurface, UINT RowPitch, DXGI_FORMAT Format) :
	m_pMappedSurface(pMappedSurface), m_RowPitch(RowPitch) 
{
	// Supports any format as long as the formats name is DXGI_FORMAT_R8G8B8A8_UNORM
	assert(Format == DXGI_FORMAT_R8G8B8A8_UNORM);
}

void RTD3DMappedCanvas::WritePixel(UINT x, UINT y, Vec3 Color)
{
	byte *pPixel = ((byte *)m_pMappedSurface) + m_RowPitch * y + x * sizeof(byte)* 4;
	pPixel[0] = Color.x;
	pPixel[1] = Color.y;
	pPixel[2] = Color.z;
	pPixel[3] = 0;
}

void RTracer::Trace(RTScene *pScene, RTCamera *pCamera, RTCanvas *pCanvas)
{
	UINT Width = pCamera->GetWidth();
	UINT Height = pCamera->GetHeight();

	for (UINT y = 0; y < Height; y++)
	{
		for (UINT x = 0; x < Width; x++)
		{
			pCanvas->WritePixel(x, y, Vec3((((float)x / (float)Width) * 255.0), (BYTE)(((float)y / (float)Height) * 255.0), 0.0f));
		}
	}
}
