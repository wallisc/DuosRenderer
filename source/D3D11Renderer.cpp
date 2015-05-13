#include "D3D11Renderer.h"
#include "RendererException.h"

D3D11Renderer::D3D11Renderer()
{
	UINT CeateDeviceFlags = 0;
#ifdef DEBUG
	CeateDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	D3D_FEATURE_LEVEL FeatureLevels = D3D_FEATURE_LEVEL_11_1;
	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, CeateDeviceFlags, &FeatureLevels, 1,
		D3D11_SDK_VERSION, &m_pDevice, nullptr, &m_pImmediateContext);

	FAIL_CHK(FAILED(hr), "Failed D3D11CreateDevice");
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

