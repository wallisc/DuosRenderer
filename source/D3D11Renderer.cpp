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

void D3D11Renderer::CreateGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor, _Out_ Geometry **ppGeometry)
{
	*ppGeometry = new D3D11Geometry(m_pDevice, pCreateGeometryDescriptor);
	MEM_CHK(ppGeometry);

}

void D3D11Renderer::DestroyGeometry(_In_ Geometry *pGeometry)
{
	delete pGeometry;
}

void D3D11Renderer::CreateLight(_In_ CreateLightDescriptor *pCreateLightDescriptor, _Out_ Light **ppLight)
{
	*ppLight = new D3D11Light(pCreateLightDescriptor);
	MEM_CHK(ppLight);
}

void D3D11Renderer::DrawScene(Camera *pCamera, Scene *pScene)
{
}

D3D11Geometry::D3D11Geometry(_In_ ID3D11Device *pDevice, _In_ CreateGeometryDescriptor *pCreateGeometryDescriptor) :
	m_pIndexBuffer(0), m_pVertexBuffer(0)
{
	HRESULT hr = S_OK;
	m_UsesIndexBuffer = pCreateGeometryDescriptor->m_NumIndices != 0;
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