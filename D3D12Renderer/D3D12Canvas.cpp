#include "stdafx.h"

D3D12Canvas::D3D12Canvas(ID3D12Resource *pResource, D3D12_RESOURCE_STATES state) : 
	m_pResource(pResource),
	m_ResourceState(state)
{}

D3D12_RESOURCE_STATES D3D12Canvas::GetResourceSate()
{
	return m_ResourceState;
}

ID3D12Resource &D3D12Canvas::GetCanvasResource()
{
	return *m_pResource;
}

void D3D12Canvas::GetInterface(const char *InterfaceName, void **ppInterface)
{
	if (strcmp(D3D12CanvasInterfaceName, InterfaceName) == 0)
	{
		*ppInterface = this;
	}
}

void D3D12Canvas::WritePixel(unsigned int x, unsigned int y, DuosRenderer::Vec3 Color)
{
	assert(false); // Not implemented
}

std::shared_ptr<DuosRenderer::Canvas> CreateD3D12Canvas(ID3D12Resource *pResource, D3D12_RESOURCE_STATES state)
{
	return std::make_shared<D3D12Canvas>(pResource, state);
}