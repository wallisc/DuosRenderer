#include "stdafx.h"

#include "WicTextureLoader.h"
#include "ResourceUploadBatch.h"

D3D12EnvironmentMap::D3D12EnvironmentMap(ID3D12CommandQueue &queue, CreateEnvironmentMapDescriptor &desc)
{
	assert(desc.m_EnvironmentType == CreateEnvironmentMapDescriptor::TEXTURE_PANORAMA); // Only supporting pano's right now
	CComPtr<ID3D12Device> pDevice;
	ThrowIfFailed(queue.GetDevice(IID_PPV_ARGS(&pDevice)));
	m_pDevice = pDevice.p;

	DirectX::ResourceUploadBatch uploadBatch(m_pDevice);
	uploadBatch.Begin();

	std::string asciiStr = desc.m_TexturePanorama.m_TextureName;
	std::wstring wideString = std::wstring(asciiStr.begin(), asciiStr.end());
	ThrowIfFailed(DirectX::CreateWICTextureFromFile(m_pDevice, uploadBatch, wideString.c_str(), &m_pPanoramaTexture));

	uploadBatch.End(&queue);
}