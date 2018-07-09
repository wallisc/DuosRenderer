#pragma once
#include "D3D12RendererInternal.h"

class D3D12Geometry : public Geometry
{
public:
	D3D12Geometry(D3D12Context &context, CreateGeometryDescriptor &desc);

	virtual Material &GetMaterial() const { return *m_pMaterial; };
	virtual D3D12Material &GetD3D12Material() const { return *m_pMaterial; };

	virtual void Translate(_In_ const Vec3 &translationVector) { assert(false); }
	virtual void Rotate(float row, float yaw, float pitch) { assert(false); }

	D3D12_RAYTRACING_GEOMETRY_DESC GetGeometryDesc();

	D3D12_GPU_DESCRIPTOR_HANDLE GetGeometryDescriptorTable()
	{
		return m_DescriptorTable;
	}
	enum GeometryDescriptorTable
	{
		GeometryDescriptorTableIndexBufferSlot = 0,
		GeometryDescriptorTableAttributeBufferSlot,
		GeometryDescriptorTableSize,
	};

private:
	D3D12Descriptor m_DescriptorTable;
	std::shared_ptr<D3D12Material> m_pMaterial;

	CComPtr<ID3D12Resource> m_pAttributeBuffer;
	CComPtr<ID3D12Resource> m_pVertexBuffer;
	CComPtr<ID3D12Resource> m_pIndexBuffer;
};