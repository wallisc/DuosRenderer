#include "stdafx.h"

#include "WicTextureLoader.h"
#include "ResourceUploadBatch.h"

D3D12EnvironmentMap::D3D12EnvironmentMap(D3D12Context &context, CreateEnvironmentMapDescriptor &desc)
{
	assert(desc.m_EnvironmentType == CreateEnvironmentMapDescriptor::TEXTURE_PANORAMA); // Only supporting pano's right now
	m_pDevice = &context.GetDevice();

	DirectX::ResourceUploadBatch uploadBatch(m_pDevice);
	uploadBatch.Begin();

	std::string asciiStr = desc.m_TexturePanorama.m_TextureName;
	std::wstring wideString = std::wstring(asciiStr.begin(), asciiStr.end());
	ThrowIfFailed(DirectX::CreateWICTextureFromFile(m_pDevice, uploadBatch, wideString.c_str(), &m_pPanoramaTexture));

	uploadBatch.End(&context.GetCommandQueue());

	auto &panoramaDesc = m_pPanoramaTexture->GetDesc();
	m_EnviromentMapDescriptor = context.GetDescriptorAllocator().AllocateDescriptor();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	m_pDevice->CreateShaderResourceView(m_pPanoramaTexture, &srvDesc, m_EnviromentMapDescriptor);
}