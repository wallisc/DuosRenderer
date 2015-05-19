#include "D3D11Renderer.h"
#include "RendererException.h"

#include <d3dcompiler.h>
#include <atlbase.h>

using namespace DirectX;

enum VectorType
{
	DIRECTION,
	POSITION
};

XMVECTOR RealArrayToXMVector(_In_reads_(3) REAL *pArray, VectorType Type)
{
	return XMVectorSet(pArray[X_INDEX], pArray[Y_INDEX], pArray[Z_INDEX], Type == DIRECTION ? (REAL)0.0 : (REAL)1.0);
}

//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DCompile
//
// With VS 11, we could load up prebuilt .cso files instead...
//--------------------------------------------------------------------------------------
HRESULT CompileShaderHelper(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
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

D3D11Renderer::D3D11Renderer(HWND WindowHandle, unsigned int width, unsigned int height)
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

void D3D11Renderer::CompileShaders()
{
	// Compile the forward rendering shaders
	{
		CComPtr<ID3DBlob> pVSBlob = nullptr;
		HRESULT hr = CompileShaderHelper(L"GeometryRender.fx", "VS", "vs_4_0", &pVSBlob);
		FAIL_CHK(FAILED(hr), "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.");

		hr = m_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_pForwardVertexShader);
		FAIL_CHK(FAILED(hr), "Failed to CreateVertexShader");

		// Define the input layout
		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		UINT numElements = ARRAYSIZE(layout);

		hr = m_pDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
			pVSBlob->GetBufferSize(), &m_pForwardInputLayout);

		CComPtr<ID3DBlob> pPSBlob = nullptr;
		hr = CompileShaderHelper(L"GeometryRender.fx", "PS", "ps_4_0", &pPSBlob);
		FAIL_CHK(FAILED(hr), "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.");

		hr = m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pForwardPixelShader);
		FAIL_CHK(FAILED(hr), "Failed to CreatePixelShader");
	}
}

void D3D11Renderer::InitializeSwapchain(HWND WindowHandle, unsigned int width, unsigned int height)
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
	FAIL_CHK(dxgiFactory2, "DXGIFactory2 is null");
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

	m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

Geometry* D3D11Renderer::CreateGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor)
{
	D3D11Geometry* pGeometry = new D3D11Geometry(m_pDevice, pCreateGeometryDescriptor);
	MEM_CHK(pGeometry);
	return pGeometry;
}

void D3D11Renderer::DestroyGeometry(_In_ Geometry *pGeometry)
{
	delete pGeometry;
}

Light *D3D11Renderer::CreateLight(_In_ CreateLightDescriptor *pCreateLightDescriptor)
{
	D3D11Light *pLight = new D3D11Light(pCreateLightDescriptor);
	MEM_CHK(pLight);
	return pLight;
}

void D3D11Renderer::DestroyLight(Light *pLight)
{
	delete pLight;
}

Camera *D3D11Renderer::CreateCamera(_In_ CreateCameraDescriptor *pCreateCameraDescriptor)
{
	Camera *pCamera = new D3D11Camera(m_pDevice, pCreateCameraDescriptor);
	MEM_CHK(pCamera);
	return pCamera;
}

void D3D11Renderer::DestroyCamera(Camera *pCamera)
{
	delete pCamera;
}

Scene *D3D11Renderer::CreateScene()
{
	Scene *pScene = new D3D11Scene();
	MEM_CHK(pScene);
	return pScene;
}

void D3D11Renderer::DestroyScene(Scene* pScene)
{
	delete pScene;
}

void D3D11Renderer::DrawScene(Camera *pCamera, Scene *pScene)
{
	D3D11Scene *pD3D11Scene = D3D11_RENDERER_CAST<D3D11Scene*>(pScene);
	const unsigned int Strides = sizeof(Vertex);
	const unsigned int Offsets = 0;
	for (auto pGeometry : pD3D11Scene->m_GeometryList)
	{
		ID3D11Buffer *pVertexBuffer = pGeometry->GetVertexBuffer();
		ID3D11Buffer *pIndexBuffer = pGeometry->GetIndexBuffer();
		m_pImmediateContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &Strides, &Offsets);
		m_pImmediateContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
		if (pIndexBuffer)
		{
			m_pImmediateContext->DrawIndexed(pGeometry->GetIndexCount(), 0, 0);
		}
		else
		{
			m_pImmediateContext->Draw(pGeometry->GetVertexCount(), 0);
		}
	}
}

D3D11Geometry::D3D11Geometry(_In_ ID3D11Device *pDevice, _In_ CreateGeometryDescriptor *pCreateGeometryDescriptor) :
	m_pIndexBuffer(0), m_pVertexBuffer(0)
{
	HRESULT hr = S_OK;
	m_UsesIndexBuffer = pCreateGeometryDescriptor->m_NumIndices != 0;
	m_VertexCount = pCreateGeometryDescriptor->m_NumVertices;
	m_IndexCount = pCreateGeometryDescriptor->m_NumIndices;
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
		FAIL_CHK(SUCCEEDED(hr), "Failed to create Index Buffer");
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
	FAIL_CHK(SUCCEEDED(hr), "Failed to create Index Buffer");
}

D3D11Geometry::~D3D11Geometry()
{
	m_pVertexBuffer->Release();
	if (m_pIndexBuffer) { m_pIndexBuffer->Release(); };
}

void D3D11Geometry::Update(_In_ Transform *pTransform) 
{
	assert(false); // Implement
}

D3D11Light::D3D11Light(_In_ CreateLightDescriptor *pCreateLightDescriptor)
{
	switch (pCreateLightDescriptor->m_LightType)
	{
	default:
		assert(false);
		break;
	}
}

void D3D11Scene::AddGeometry(_In_ Geometry *pGeometry)
{
	D3D11Geometry *pD3D11Geometry = D3D11_RENDERER_CAST<D3D11Geometry*>(pGeometry);
	m_GeometryList.push_back(pD3D11Geometry);
}

D3D11Camera::D3D11Camera(ID3D11Device *pDevice, CreateCameraDescriptor *pCreateCameraDescriptor)
{
	m_Width = pCreateCameraDescriptor->m_Width;
	m_Height = pCreateCameraDescriptor->m_Height;

	XMMATRIX Projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, 
		pCreateCameraDescriptor->m_Width / pCreateCameraDescriptor->m_Height, 
		pCreateCameraDescriptor->m_NearClip, 
		pCreateCameraDescriptor->m_FarClip);

	XMVECTOR Eye = RealArrayToXMVector(pCreateCameraDescriptor->m_Position, POSITION);
	XMVECTOR At = RealArrayToXMVector(pCreateCameraDescriptor->m_LookAt, POSITION);
	XMVECTOR Up = RealArrayToXMVector(pCreateCameraDescriptor->m_Up, DIRECTION);
	XMMATRIX View = XMMatrixLookAtLH(Eye, At, Up);

	m_ViewMatrix = View * Projection;
}

D3D11Camera::~D3D11Camera()
{

}

void D3D11Camera::Update(_In_ Transform *pTransform)
{
	assert(false);
}


