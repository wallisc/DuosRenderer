#pragma once
#include "D3D12RendererInternal.h"

class D3D12Material : public Material
{
public:
	D3D12Material(D3D12Context &context, CreateMaterialDescriptor &desc);

	virtual ~D3D12Material() {};
	virtual float GetRoughness() const { return m_Roughness; }
	virtual float GetReflectivity() const { return m_Reflectivity; }

	D3D12_GPU_DESCRIPTOR_HANDLE GetDescriptorTable() { return m_DescriptorTable; }

	virtual void SetRoughness(float Roughness) { m_Roughness = Roughness; assert(false); /*Need to update GPU resource*/ }
	virtual void SetReflectivity(float Reflectivity) { m_Reflectivity = Reflectivity; assert(false); /*Need to update GPU resource*/ }

	enum MaterialDescriptorTable
	{
		MaterialDescriptorTableDiffuseTextureSlot = 0,
		MaterialDescriptorTableMaterialConstantsSlot,
		MaterialDescriptorTableSize,
	};

private:
	CComPtr<ID3D12Resource> m_pDiffuseTexture;
	CComPtr<ID3D12Resource> m_pMaterialConstants;
	D3D12Descriptor m_DescriptorTable;
	float m_Roughness;
	float m_Reflectivity;
};