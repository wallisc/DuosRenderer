#include "D3D11Renderer.h"
#include "D3D11Util.h"
#include "RendererException.h"
#include "D3D11Canvas.h"

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
	hr = D3DCompileFromFile(szFileName, pDefines, nullptr, szEntryPoint, szShaderModel,
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
	UINT CeateDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef DEBUG
	CeateDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	D3D_FEATURE_LEVEL FeatureLevels = D3D_FEATURE_LEVEL_11_1;
	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, CeateDeviceFlags, &FeatureLevels, 1,
		D3D11_SDK_VERSION, &m_pDevice, nullptr, &m_pImmediateContext);
	FAIL_CHK(FAILED(hr), "Failed D3D11CreateDevice");

	hr = m_pDevice->QueryInterface(__uuidof(ID3D11Device1), (void**)&m_pDevice1);
	FAIL_CHK(FAILED(hr), "Failed query for ID3D11Device1");

	InitializeSwapchain(WindowHandle, width, height);

	CompileShaders();
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
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		UINT numElements = ARRAYSIZE(layout);

		hr = m_pDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
			pVSBlob->GetBufferSize(), &m_pForwardInputLayout);

		D3D_SHADER_MACRO macros[2] = {};
		macros[0].Name = "USE_TEXTURE";
		macros[0].Definition = "1";

		CComPtr<ID3DBlob> pPSBlob = nullptr;
		hr = CompileShaderHelper(L"GeometryRender.fx", "PS", "ps_4_0", macros, &pPSBlob);
		FAIL_CHK(FAILED(hr), "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.");

		hr = m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pForwardTexturedPixelShader);
		FAIL_CHK(FAILED(hr), "Failed to CreatePixelShader");


		macros[0].Name = "USE_TEXTURE";
		macros[0].Definition = "0";
		pPSBlob = nullptr;
		hr = CompileShaderHelper(L"GeometryRender.fx", "PS", "ps_4_0", macros, &pPSBlob);
		FAIL_CHK(FAILED(hr), "The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.");

		hr = m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pForwardMattePixelShader);
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

D3D11Material::D3D11Material(_In_ ID3D11Device *pDevice, CreateMaterialDescriptor *pCreateMaterialDescriptor)
{
	HRESULT result = S_OK;
	if (pCreateMaterialDescriptor->m_TextureName && strlen(pCreateMaterialDescriptor->m_TextureName))
	{
		CA2WEX<MAX_ALLOWED_STR_LENGTH> WideTextureName(pCreateMaterialDescriptor->m_TextureName);
		result = CreateWICTextureFromFile(pDevice,
			WideTextureName,
			nullptr,
			&pTextureResourceView,
			20 * 1024 * 1024);
		FAIL_CHK(FAILED(result), "Failed to create SRV for texture");
	}
	else
	{
		pTextureResourceView = nullptr;
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


D3D11EnvironmentTextureCube::D3D11EnvironmentTextureCube(
	_In_ ID3D11Device *pDevice, 
	_In_ ID3D11DeviceContext *pImmediateContext,
	_In_ const CreateEnvironmentTextureCube *pCreateTextureCube)
{
	m_pEnvironmentTextureCube = CreateTextureCube(pDevice, pImmediateContext, pCreateTextureCube->m_TextureNames);
	m_pIrradianceTextureCube = CreateTextureCube(pDevice, pImmediateContext, pCreateTextureCube->m_IrradianceTextureNames);

	D3D11_BUFFER_DESC CameraVertexBufferDesc;
	ZeroMemory(&CameraVertexBufferDesc, sizeof(CameraVertexBufferDesc));
	CameraVertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	CameraVertexBufferDesc.ByteWidth = sizeof(CameraPlaneVertex)* VerticesInCameraPlane;
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
		HRESULT hr = CreateWICTextureFromFile(pDevice,
			WideTextureName,
			&pTempResource,
			nullptr,
			20 * 1024 * 1024);
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

	CameraPlaneVertex *pVertexBuffer = (CameraPlaneVertex *) MappedSubresource.pData;

	const XMVECTOR viewRight = pCamera->GetRight();
	 XMVECTOR viewUp = pCamera->GetUp();
	 XMVECTOR viewDirection = pCamera->GetViewDirection();
	float horizontalFOV = pCamera->GetHorizontalFieldOfView();
	float verticalFOV = pCamera->GetVerticalFieldOfView();

	XMVECTOR rotationQuaternion;
	XMVECTOR transformedViewDirection;
	
	rotationQuaternion = XMQuaternionRotationAxis(viewUp, -horizontalFOV / 2.0f);
	transformedViewDirection = XMVector3Rotate(viewDirection, rotationQuaternion);
	rotationQuaternion = XMQuaternionRotationAxis(viewRight, verticalFOV / 2.0f);
	transformedViewDirection = XMVector3Rotate(transformedViewDirection, rotationQuaternion);
	pVertexBuffer[0].m_ViewVector = XMVectorToRealArray(transformedViewDirection);
	pVertexBuffer[0].m_Position = Vec3(-1.0, 1.0, 0.0);

	rotationQuaternion = XMQuaternionRotationAxis(viewUp, horizontalFOV / 2.0);
	transformedViewDirection = XMVector3Rotate(viewDirection, rotationQuaternion);
	rotationQuaternion = XMQuaternionRotationAxis(viewRight, verticalFOV / 2.0);
	transformedViewDirection = XMVector3Rotate(transformedViewDirection, rotationQuaternion);
	pVertexBuffer[1].m_ViewVector = XMVectorToRealArray(transformedViewDirection);
	pVertexBuffer[1].m_Position = Vec3(1.0, 1.0, 0.0);

	rotationQuaternion = XMQuaternionRotationAxis(viewUp, -horizontalFOV / 2.0f);
	transformedViewDirection = XMVector3Rotate(viewDirection, rotationQuaternion);
	rotationQuaternion = XMQuaternionRotationAxis(viewRight, -verticalFOV / 2.0f);
	transformedViewDirection = XMVector3Rotate(transformedViewDirection, rotationQuaternion);
	pVertexBuffer[2].m_ViewVector = XMVectorToRealArray(transformedViewDirection);
	pVertexBuffer[2].m_Position = Vec3(-1.0, -1.0, 0.0);

	pVertexBuffer[3] = pVertexBuffer[2];
	pVertexBuffer[4] = pVertexBuffer[1];
	rotationQuaternion = XMQuaternionRotationAxis(viewUp, horizontalFOV / 2.0f);
	transformedViewDirection = XMVector3Rotate(viewDirection, rotationQuaternion);
	rotationQuaternion = XMQuaternionRotationAxis(viewRight, -verticalFOV / 2.0f);
	transformedViewDirection = XMVector3Rotate(transformedViewDirection, rotationQuaternion);
	pVertexBuffer[5].m_ViewVector = XMVectorToRealArray(transformedViewDirection);
	pVertexBuffer[5].m_Position = Vec3(1.0, -1.0, 0.0);

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
		pEnvironmentMap = new D3D11EnvironmentColor(m_pDevice, &pCreateEnvironmnetMapDescriptor->m_SolidColor);
		break;
	case CreateEnvironmentMapDescriptor::TEXTURE_CUBE:
		pEnvironmentMap = new D3D11EnvironmentTextureCube(m_pDevice, m_pImmediateContext, &pCreateEnvironmnetMapDescriptor->m_TextureCube);
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
		ID3D11ShaderResourceView *pEnvironmentMap,
		ID3D11ShaderResourceView *pIrradianceMap,
		ID3D11RenderTargetView *pRenderTarget, 
		ID3D11DepthStencilView *pDepthBuffer) :
		m_pDepthBuffer(pDepthBuffer), 
		m_pViewProjBuffer(pViewProjBuffer), 
		m_pLightViewProjBuffer(pLightViewProjBuffer),
		m_pRenderTarget(pRenderTarget),
		m_pShadowView(pShadowView),
		m_pEnvironmentMap(pEnvironmentMap),
		m_pIrradianceMap(pIrradianceMap) {}

	void SetBindings(ID3D11DeviceContext *pContext)
	{
		pContext->OMSetRenderTargets(1, &m_pRenderTarget, m_pDepthBuffer);
		pContext->VSSetConstantBuffers(1, 1, &m_pViewProjBuffer);
		pContext->PSSetConstantBuffers(1, 1, &m_pViewProjBuffer);
		pContext->VSSetConstantBuffers(3, 1, &m_pLightViewProjBuffer);
		pContext->PSSetShaderResources(1, 1, &m_pShadowView);
		pContext->PSSetShaderResources(2, 1, &m_pEnvironmentMap);
		pContext->PSSetShaderResources(3, 1, &m_pIrradianceMap);
	}

private:
	ID3D11Buffer *m_pViewProjBuffer;
	ID3D11Buffer *m_pLightViewProjBuffer;
	ID3D11RenderTargetView* m_pRenderTarget;
	ID3D11DepthStencilView* m_pDepthBuffer;
	ID3D11ShaderResourceView *m_pShadowView;
	ID3D11ShaderResourceView *m_pEnvironmentMap;
	ID3D11ShaderResourceView *m_pIrradianceMap;
};

void D3D11Renderer::DrawScene(Camera *pCamera, Scene *pScene)
{
	D3D11Scene *pD3D11Scene = D3D11_RENDERER_CAST<D3D11Scene*>(pScene);
	D3D11Camera *pD3D11Camera = D3D11_RENDERER_CAST<D3D11Camera*>(pCamera);

	for (auto pLight : pD3D11Scene->GetDirectionalLightList())
	{
		pLight->UpdateLight(m_pImmediateContext, pD3D11Camera->GetInvTransViewMatrix());
	}

	pD3D11Scene->GetEnvironmentMap()->DrawEnvironmentMap(m_pImmediateContext, pD3D11Camera, m_pSwapchainRenderTargetView);
	m_pImmediateContext->ClearDepthStencilView(m_pDepthBuffer, D3D11_CLEAR_DEPTH, 1.0f, 0);
	m_pImmediateContext->ClearDepthStencilView(m_pShadowDepthBuffer, D3D11_CLEAR_DEPTH, 1.0f, 0);

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
		pD3D11Scene->GetEnvironmentMap()->GetEnvironmentMapSRV(),
		pD3D11Scene->GetEnvironmentMap()->GetIrradianceMapSRV(),
		m_pSwapchainRenderTargetView, m_pDepthBuffer);
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

			if (pGeometry->GetD3D11Material()->HasTexture())
			{
				ID3D11ShaderResourceView *pDiffuseTexture = pGeometry->GetD3D11Material()->GetShaderResourceView();
				m_pImmediateContext->PSSetShaderResources(0, 1, &pDiffuseTexture);
				m_pImmediateContext->PSSetShader(m_pForwardTexturedPixelShader, NULL, 0);
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
	}
	

	ID3D11ShaderResourceView *NullViews[4] = {};
	m_pImmediateContext->PSSetShaderResources(0, ARRAYSIZE(NullViews), NullViews);
	m_pImmediateContext->Flush();
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
	m_pViewProjBuffer = CreateConstantBuffer(pDevice, m_pLightConstantBuffer, sizeof(CBViewProjectionTransforms));
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
	m_HorizontalFieldOfView = 2.0 * atan(tan((double)m_VerticalFieldOfView / 2.0) * (double)GetAspectRatio());

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