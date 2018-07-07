#pragma once
class D3D12Canvas : public DuosRenderer::Canvas
{
public:
	D3D12Canvas(ID3D12Resource *pResource, D3D12_RESOURCE_STATES state);
	ID3D12Resource &GetCanvasResource();
	D3D12_RESOURCE_STATES GetResourceSate();
	virtual void GetInterface(const char *InterfaceName, void **ppInterface);
	virtual void WritePixel(unsigned int x, unsigned int y, DuosRenderer::Vec3 Color);

private:
	CComPtr<ID3D12Resource> m_pResource;
	D3D12_RESOURCE_STATES m_ResourceState;
};
const static LPCSTR D3D12CanvasInterfaceName = "D3D12Canvas";