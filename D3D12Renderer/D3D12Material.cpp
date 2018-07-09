#include "stdafx.h"
#include "WicTextureLoader.h"
#include "ResourceUploadBatch.h"

D3D12Material::D3D12Material(D3D12Context &context, CreateMaterialDescriptor &desc) : 
	m_Roughness(desc.m_Roughness),
	m_Reflectivity(desc.m_Reflectivity)
{
	auto &device = context.GetDevice();
	DirectX::ResourceUploadBatch uploadBatch(&device);
	uploadBatch.Begin();

	m_DescriptorTable = context.GetDescriptorAllocator().AllocateDescriptor(MaterialDescriptorTableSize);
	if (desc.m_TextureName)
	{
		std::string asciiStr = desc.m_TextureName;
		std::wstring wideString = std::wstring(asciiStr.begin(), asciiStr.end());
		ThrowIfFailed(DirectX::CreateWICTextureFromFile(&device, uploadBatch, wideString.c_str(), &m_pDiffuseTexture));

		uploadBatch.End(&context.GetCommandQueue());
		device.CreateShaderResourceView(m_pDiffuseTexture, nullptr, m_DescriptorTable + MaterialDescriptorTableDiffuseTextureSlot);
	}

	MaterialConstants constants;
	constants.HasDiffuseTexture = m_pDiffuseTexture != nullptr;
	constants.DiffuseColor = { desc.m_DiffuseColor.x, desc.m_DiffuseColor.y, desc.m_DiffuseColor.z, 0.0f };
	constants.Reflectivity = m_Reflectivity;
	constants.Roughness = m_Roughness;

	context.UploadData(&constants, sizeof(constants), &m_pMaterialConstants, true);
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_pMaterialConstants->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = m_pMaterialConstants->GetDesc().Width;
	device.CreateConstantBufferView(&cbvDesc, m_DescriptorTable + MaterialDescriptorTableMaterialConstantsSlot);
}
