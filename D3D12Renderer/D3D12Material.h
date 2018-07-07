#pragma once
#include "D3D12RendererInternal.h"

class D3D12Material : public Material
{
public:
	D3D12Material(CreateMaterialDescriptor &desc) : 
		m_Reflectivity(desc.m_Reflectivity),
		m_Roughness(desc.m_Roughness)
	{
		assert(false); // Ignoring the diffuse texture...
	}

	virtual ~D3D12Material() {};
	virtual float GetRoughness() const { return m_Roughness; }
	virtual float GetReflectivity() const { return m_Reflectivity; }

	virtual void SetRoughness(float Roughness) { m_Roughness = Roughness; }
	virtual void SetReflectivity(float Reflectivity) { m_Reflectivity = Reflectivity; }

private:
	float m_Roughness;
	float m_Reflectivity;
};