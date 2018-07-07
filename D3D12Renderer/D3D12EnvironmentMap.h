#pragma once
#include "D3D12RendererInternal.h"

class D3D12EnvironmentMap : public EnvironmentMap
{
public:
	D3D12EnvironmentMap(D3D12Context &context, CreateEnvironmentMapDescriptor &desc);
	virtual ~D3D12EnvironmentMap() {};

	const D3D12Descriptor &GetEnvironmentMapSRV() 
	{
		return m_EnviromentMapDescriptor;
	}
private:
	D3D12Descriptor m_EnviromentMapDescriptor;
	ID3D12Device *m_pDevice;
	CComPtr<ID3D12Resource> m_pPanoramaTexture;
};