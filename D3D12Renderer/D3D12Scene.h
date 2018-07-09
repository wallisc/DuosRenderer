#pragma once
#include "D3D12RendererInternal.h"

class D3D12Scene : public Scene
{
public:
	virtual ~D3D12Scene() {}
	D3D12Scene(D3D12Context &context, ID3D12RaytracingFallbackStateObject &stateObject, std::shared_ptr<EnvironmentMap> pEnvironmentMap) :
		m_bRebuildTopLevelAccelerationStructure(false),
		m_Context(context),
		m_stateObject(stateObject),
		m_pBottomLevelAccelerationStructure(&context),
		m_pTopLevelAccelerationStructure(&context)
	{
		m_pEnvironmentMap = safe_dynamic_cast<D3D12EnvironmentMap>(pEnvironmentMap);
	}

	virtual void AddGeometry(_In_ std::shared_ptr<Geometry> pGeometry)
	{
		m_bRebuildTopLevelAccelerationStructure = true;
		m_pGeometry.push_back(safe_dynamic_cast<D3D12Geometry>(pGeometry));
	}
	virtual void AddLight(_In_ std::shared_ptr<Light> pLight)
	{
		m_pLights.push_back(safe_dynamic_cast<D3D12Light>(pLight));
	}

	WRAPPED_GPU_POINTER GetTopLevelAccelerationStructure()
	{
		if (m_bRebuildTopLevelAccelerationStructure)
		{
			m_TopLevelAccelerationStructurePointer = BuildTopLevelAccelerationStructure(m_Context);
			BuildShaderTables(m_Context);
			m_bRebuildTopLevelAccelerationStructure = false;
		}
		return m_TopLevelAccelerationStructurePointer;
	}

	const D3D12Descriptor &GetEnvironmentMap()
	{
		return m_pEnvironmentMap->GetEnvironmentMapSRV();
	}

	struct HitGroupShaderRecord
	{
		BYTE ShaderIdentifier[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES];
		D3D12_GPU_DESCRIPTOR_HANDLE GeometryDescriptorTable;
		BYTE Padding[24];
	};

	D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE GetHitGroupTable()
	{
		D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE hitGroupTable;
		hitGroupTable.StartAddress = m_pHitGroupShaderTable->GetGPUVirtualAddress();
		hitGroupTable.SizeInBytes = m_pHitGroupShaderTable->GetDesc().Width;
		hitGroupTable.StrideInBytes = sizeof(HitGroupShaderRecord);
		return hitGroupTable;
	}

private:
	D3D12BufferDescriptor BuildTopLevelAccelerationStructure(D3D12Context &context);
	void BuildShaderTables(D3D12Context &context);

	bool m_bRebuildTopLevelAccelerationStructure;
	DeferredDeletionUniquePtr<ID3D12Resource> m_pTopLevelAccelerationStructure;
	DeferredDeletionUniquePtr<ID3D12Resource> m_pBottomLevelAccelerationStructure;
	DeferredDeletionUniquePtr<ID3D12Resource> m_pHitGroupShaderTable;
	D3D12BufferDescriptor m_TopLevelAccelerationStructurePointer;

	D3D12Context &m_Context;
	ID3D12RaytracingFallbackStateObject &m_stateObject;
	std::shared_ptr<D3D12EnvironmentMap> m_pEnvironmentMap;
	std::vector<std::shared_ptr<D3D12Light>> m_pLights;
	std::vector<std::shared_ptr<D3D12Geometry>> m_pGeometry;
};
