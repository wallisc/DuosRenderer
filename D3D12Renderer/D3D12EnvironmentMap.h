#pragma once
#include "D3D12RendererInternal.h"

class D3D12EnvironmentMap : public EnvironmentMap
{
public:
	D3D12EnvironmentMap(ID3D12CommandQueue &queue, CreateEnvironmentMapDescriptor &desc);
	virtual ~D3D12EnvironmentMap() {};

private:
	ID3D12Device *m_pDevice;
	CComPtr<ID3D12Resource> m_pPanoramaTexture;
};