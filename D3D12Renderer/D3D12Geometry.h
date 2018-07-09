#pragma once
#include "D3D12RendererInternal.h"

class D3D12Geometry : public Geometry
{
public:
	D3D12Geometry(D3D12Context &context, CreateGeometryDescriptor &desc);

	virtual Material &GetMaterial() const { return *m_pMaterial; };

	virtual void Translate(_In_ const Vec3 &translationVector) { assert(false); }
	virtual void Rotate(float row, float yaw, float pitch) { assert(false); }

	WRAPPED_GPU_POINTER GetBottomLevelAccelerationStructure()
	{
		return m_BottomLevelAccelerationStructurePointer;
	}

	D3D12_RAYTRACING_GEOMETRY_DESC GetGeometryDesc();
private:
	void BuildAccelerationStructure(D3D12Context &context);

	D3D12BufferDescriptor m_BottomLevelAccelerationStructurePointer;
	std::shared_ptr<Material> m_pMaterial;

	CComPtr<ID3D12Resource> m_pAttributeBuffer;
	CComPtr<ID3D12Resource> m_pVertexBuffer;
	CComPtr<ID3D12Resource> m_pIndexBuffer;
	CComPtr<ID3D12Resource> m_pBottomLevelAccelerationStructure;
};