#include "D3D11Renderer.h"
#include "D3D11Util.h"
#include "RendererException.h"

#include "dxtk/inc/WICTextureLoader.h"

#include <atlconv.h>
#include <d3dcompiler.h>
#include <directxcolors.h>
#include <atlbase.h>

using namespace DirectX;

enum VectorType
{
	DIRECTION,
	POSITION
};

XMVECTOR RealArrayToXMVector(Vec3 Vector, VectorType Type)
{
	return XMVectorSet(Vector.x, Vector.y, Vector.z, Type == DIRECTION ? (REAL)0.0 : (REAL)1.0);
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
	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, CeateDeviceFlags, &FeatureLevels, 1,
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
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
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
	hr = m_pDevice->CreateTexture2D(&descDepth, nullptr, &pDepthStencil);
	FAIL_CHK(FAILED(hr), "Failed creating a depth stencil resource");

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = m_pDevice->CreateDepthStencilView(pDepthStencil, &descDSV, &m_pDepthBuffer);
	FAIL_CHK(FAILED(hr), "Failed creating a depth stencil view");

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
	m_pImmediateContext->IASetInputLayout(m_pForwardInputLayout);
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
	Camera *pCamera = new D3D11Camera(m_pDevice, pCreateCameraDescriptor);
	MEM_CHK(pCamera);
	return pCamera;
}

void D3D11Renderer::DestroyCamera(Camera *pCamera)
{
	delete pCamera;
}

D3D11Material::D3D11Material(_In_ ID3D11Device *pDevice, CreateMaterialDescriptor *pCreateMaterialDescriptor)
{
	CA2WEX<MAX_ALLOWED_STR_LENGTH> WideTextureName(pCreateMaterialDescriptor->m_TextureName);
	std::wstring wTextureName();
	HRESULT result = CreateWICTextureFromFile(pDevice,
		WideTextureName,
		nullptr,
		&pTextureResourceView,
		20 * 1024 * 1024);
	FAIL_CHK(FAILED(result), "Failed to create SRV for texture");
}

Material *D3D11Renderer::CreateMaterial(_In_ CreateMaterialDescriptor *pCreateMaterialDescriptor)
{
	D3D11Material *pMaterial = new D3D11Material(m_pDevice, pCreateMaterialDescriptor);
	MEM_CHK(pMaterial);
	return pMaterial;
}

void D3D11Renderer::DestroyMaterial(Material* pMaterial)
{
	delete pMaterial;
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
	D3D11Camera *pD3D11Camera = D3D11_RENDERER_CAST<D3D11Camera*>(pCamera);
	m_pImmediateContext->ClearRenderTargetView(m_pSwapchainRenderTargetView, Colors::Gray);
	m_pImmediateContext->ClearDepthStencilView(m_pDepthBuffer, D3D11_CLEAR_DEPTH, 1.0f, 0);
	m_pImmediateContext->ClearDepthStencilView(m_pShadowDepthBuffer, D3D11_CLEAR_DEPTH, 1.0f, 0);

	ID3D11Buffer *pCameraConstantBuffer = pD3D11Camera->GetCameraConstantBuffer();
	
	assert(pD3D11Scene->GetDirectionalLightList().size() == 1); // TODO hard coded for one directional light
	ID3D11Buffer *pDirectonalLightConstantBuffer = pD3D11Scene->GetDirectionalLightList()[0]->GetDirectionalLightConstantBuffer();
	m_pImmediateContext->PSSetConstantBuffers(2, 1, &pDirectonalLightConstantBuffer);
	m_pImmediateContext->VSSetConstantBuffers(0, 1, &pCameraConstantBuffer);

	m_pImmediateContext->PSSetShader(m_pForwardPixelShader, NULL, 0);
	m_pImmediateContext->VSSetShader(m_pForwardVertexShader, NULL, 0);

	const unsigned int Strides = sizeof(Vertex);
	const unsigned int Offsets = 0;

	enum Passes {ShadowPass = 0, BasePass, NumPasses};
	for (int PassIndex = 0; PassIndex < NumPasses; PassIndex++)
	{
		Passes Pass = (Passes)PassIndex;
		if (Pass == ShadowPass)
		{
			assert(pD3D11Scene->GetDirectionalLightList().size() == 1); // TODO hard coded for one directional light

			ID3D11Buffer *pViewProjBuffer = pD3D11Scene->GetDirectionalLightList()[0]->GetViewProjBuffer(m_pImmediateContext, pD3D11Scene, pD3D11Camera);
			m_pImmediateContext->VSSetConstantBuffers(1, 1, &pViewProjBuffer);

			m_pImmediateContext->OMSetRenderTargets(0, nullptr, m_pShadowDepthBuffer);

		}
		else if (Pass == BasePass)
		{
			ID3D11Buffer *pViewProjBuffer = pD3D11Camera->GetViewProjBuffer();
			ID3D11Buffer *pLightViewProjBuffer = pD3D11Scene->GetDirectionalLightList()[0]->GetViewProjBuffer(m_pImmediateContext, pD3D11Scene, pD3D11Camera);

			m_pImmediateContext->OMSetRenderTargets(1, &m_pSwapchainRenderTargetView, m_pDepthBuffer);
			
			m_pImmediateContext->VSSetConstantBuffers(1, 1, &pViewProjBuffer);
			m_pImmediateContext->VSSetConstantBuffers(3, 1, &pLightViewProjBuffer);

			m_pImmediateContext->PSSetShaderResources(1, 1, &m_pShadowResourceView);
		}
		else
		{
			assert(false);
		}

		for (auto pGeometry : pD3D11Scene->m_GeometryList)
		{
			ID3D11Buffer *pVertexBuffer = pGeometry->GetVertexBuffer();
			ID3D11Buffer *pIndexBuffer = pGeometry->GetIndexBuffer();
			m_pImmediateContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &Strides, &Offsets);
			m_pImmediateContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
			ID3D11ShaderResourceView *pDiffuseTexture = pGeometry->GetMaterial()->GetShaderResourceView();
			m_pImmediateContext->PSSetShaderResources(0, 1, &pDiffuseTexture);
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
	

	ID3D11ShaderResourceView *NullViews[4] = {};
	m_pImmediateContext->PSSetShaderResources(0, ARRAYSIZE(NullViews), NullViews);
	m_pSwapChain->Present(0, 0);
}

D3D11Geometry::D3D11Geometry(_In_ ID3D11Device *pDevice, _In_ CreateGeometryDescriptor *pCreateGeometryDescriptor) :
	m_pIndexBuffer(0), m_pVertexBuffer(0)
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

void D3D11Geometry::Update(_In_ Transform *pTransform) 
{
	assert(false); // Implement
}

D3D11DirectionalLight::D3D11DirectionalLight(
	_In_ ID3D11Device *pDevice, 
	_In_ CreateLightDescriptor *pCreateLight) :
	D3D11Light(CreateLightDescriptor::DIRECTIONAL_LIGHT)
{
	assert(pCreateLight->m_LightType == CreateLightDescriptor::DIRECTIONAL_LIGHT);

	// Flip direction since we want light direction, not emission direction
	m_CpuLightData.m_Direction = 
		-1.0f * XMVector3Normalize(RealArrayToXMVector(
		pCreateLight->m_pCreateDirectionalLight->m_EmissionDirection, DIRECTION));
	m_CpuLightData.m_Color = RealArrayToXMVector(pCreateLight->m_Color, DIRECTION);

	m_pLightConstantBuffer = CreateConstantBuffer(pDevice, &m_CpuLightData, sizeof(CBDirectionalLight));
	
	// We need scene information to know where we should render the shadow map but we can at least 
	// allocate the buffer now
	m_ViewProjCpuData.m_Projection = XMMatrixIdentity();
	m_ViewProjCpuData.m_View = XMMatrixIdentity();
	m_pViewProjBuffer = CreateConstantBuffer(pDevice, m_pLightConstantBuffer, sizeof(CBViewProjectionTransforms));
}

ID3D11Buffer *D3D11DirectionalLight::GetViewProjBuffer(ID3D11DeviceContext *pContext, D3D11Scene *pScene, D3D11Camera *pCamera)
{
	// Calculate what's the biggest the scene can actually be.
	float MaxSceneLength = XMVectorGetX(XMVector3Length(pScene->GetMaxDimensions() - pScene->GetMinDimensions()));
	XMVECTOR SceneCenter = pScene->GetMaxDimensions() + pScene->GetMinDimensions();
	XMVECTOR EyePosition = SceneCenter + m_CpuLightData.m_Direction * MaxSceneLength;

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

	m_ViewProjCpuData.m_View = XMMatrixLookAtLH(EyePosition, SceneCenter, Up);
	m_ViewProjCpuData.m_Projection = XMMatrixOrthographicLH(
		MaxSceneLength,
		MaxSceneLength / pCamera->GetAspectRatio(),
		0.0001f,
		MaxSceneLength * 2.0f);

	pContext->UpdateSubresource(m_pViewProjBuffer, 0, nullptr, &m_ViewProjCpuData, 0, 0);
	return m_pViewProjBuffer;
}


D3D11Scene::D3D11Scene()
{
	m_MaxDimensions = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.0f);
	m_MinDimensions = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.0f);
}

void D3D11Scene::AddGeometry(_In_ Geometry *pGeometry)
{
	D3D11Geometry *pD3D11Geometry = D3D11_RENDERER_CAST<D3D11Geometry*>(pGeometry);
	
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
	D3D11Light *pD3D11Light = D3D11_RENDERER_CAST<D3D11Light*>(pLight);
	switch (pD3D11Light->GetLightType())
	{
	case CreateLightDescriptor::DIRECTIONAL_LIGHT:
		assert(m_DirectionalLightList.size() == 0); // TODO: Only supporting 1 directional light for now
		m_DirectionalLightList.push_back(D3D11_RENDERER_CAST<D3D11DirectionalLight*>(pLight));
		break;
	default:
		assert(false);
		break;
	}
}

D3D11Camera::D3D11Camera(ID3D11Device *pDevice, CreateCameraDescriptor *pCreateCameraDescriptor)
{
	m_Width = pCreateCameraDescriptor->m_Width;
	m_Height = pCreateCameraDescriptor->m_Height;

	XMVECTOR Eye = RealArrayToXMVector(pCreateCameraDescriptor->m_FocalPoint, POSITION);
	XMVECTOR At = RealArrayToXMVector(pCreateCameraDescriptor->m_LookAt, POSITION);
	XMVECTOR Up = RealArrayToXMVector(pCreateCameraDescriptor->m_Up, DIRECTION);

	m_CameraCpuData.m_CamPos = Eye;
	m_CameraCpuData.m_ClipDistance = XMVectorSet(pCreateCameraDescriptor->m_FarClip, 0, 0, 0);
	m_CameraCpuData.m_Dimensions = XMVectorSet(pCreateCameraDescriptor->m_Width, pCreateCameraDescriptor->m_Height, 0, 0);
	
	m_ViewProjCpuData.m_Projection = XMMatrixPerspectiveFovLH(pCreateCameraDescriptor->m_FieldOfView,
		pCreateCameraDescriptor->m_Width / pCreateCameraDescriptor->m_Height,
		pCreateCameraDescriptor->m_NearClip,
	 	pCreateCameraDescriptor->m_FarClip);
	m_ViewProjCpuData.m_View = XMMatrixLookAtLH(Eye, At, Up);

	m_pCameraBuffer = CreateConstantBuffer(pDevice, &m_CameraCpuData, sizeof(CBCamera));
	m_pViewProjBuffer = CreateConstantBuffer(pDevice, &m_ViewProjCpuData, sizeof(CBViewProjectionTransforms));
}

D3D11Camera::~D3D11Camera()
{

}

void D3D11Camera::Update(_In_ Transform *pTransform)
{
	assert(false);
}

ID3D11Buffer* D3D11Camera::GetCameraConstantBuffer()
{
	return m_pCameraBuffer;
}

ID3D11Buffer* D3D11Camera::GetViewProjBuffer()
{
	return m_pViewProjBuffer;
}

