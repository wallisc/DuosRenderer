#include "stdafx.h"

void D3D12Scene::BuildShaderTables(D3D12Context &context)
{
	static_assert(sizeof(HitGroupShaderRecord) % D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT == 0, L"Unaligned shader record size");

	std::vector<HitGroupShaderRecord> shaderTable;
	shaderTable.reserve(m_pGeometry.size());
	for (auto pGeometry : m_pGeometry)
	{
		HitGroupShaderRecord shaderRecord = {};
		memcpy(shaderRecord.ShaderIdentifier, m_stateObject.GetShaderIdentifier(HitGroupExportName), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
		shaderRecord.GeometryDescriptorTable = pGeometry->GetGeometryDescriptorTable();
		shaderRecord.Dummy = CD3DX12_GPU_DESCRIPTOR_HANDLE(pGeometry->GetGeometryDescriptorTable()).Offset(context.GetDevice().GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		shaderTable.push_back(shaderRecord);
	}

	m_pHitGroupShaderTable = DeferredDeletionUniquePtr<ID3D12Resource>(&context);
	context.UploadData(shaderTable.data(), shaderTable.size() * sizeof(shaderTable[0]), &m_pHitGroupShaderTable);
}


D3D12BufferDescriptor D3D12Scene::BuildTopLevelAccelerationStructure(D3D12Context &context)
{
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;
	geometryDescs.reserve(m_pGeometry.size());

	for (auto &pGeometry : m_pGeometry)
	{
		geometryDescs.push_back(pGeometry->GetGeometryDesc());
	}

	DeferDeletedCommandListAllocatorPair commandListAllocatorPair(context, context.CreateCommandListAllocatorPair());
	auto pCommandList = commandListAllocatorPair.m_pCommandList;
	auto &raytracingDevice = context.GetRaytracingDevice();
	auto &device = context.GetDevice();
	CComPtr<ID3D12RaytracingFallbackCommandList> pRaytracingCommandList;
	raytracingDevice.QueryRaytracingCommandList(pCommandList, IID_PPV_ARGS(&pRaytracingCommandList));

	ID3D12DescriptorHeap *pHeaps[] = { &context.GetDescriptorAllocator().GetDescriptorHeap() };
	pRaytracingCommandList->SetDescriptorHeaps(ARRAYSIZE(pHeaps), pHeaps);

	WRAPPED_GPU_POINTER bottomLevelPointer;
	{
		// Get required sizes for an acceleration structure.
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS &bottomLevelInput = bottomLevelBuildDesc.Inputs;
		bottomLevelInput.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		bottomLevelInput.Flags = buildFlags;
		bottomLevelInput.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		bottomLevelInput.NumDescs = static_cast<UINT>(geometryDescs.size());
		bottomLevelInput.pGeometryDescs = geometryDescs.data();

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo;
		raytracingDevice.GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInput, &bottomLevelPrebuildInfo);
		assert(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

		DeferredDeletionUniquePtr<ID3D12Resource> pScratchResource(&context);
		AllocateUAVBuffer(
			device,
			bottomLevelPrebuildInfo.ScratchDataSizeInBytes,
			&pScratchResource,
			L"AS Build Scratch Buffer");

		bottomLevelPointer = context.GetDescriptorAllocator().AllocateAccelerationStructureBuffer(
			bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes,
			&m_pBottomLevelAccelerationStructure);

		// Bottom Level Acceleration Structure desc
		bottomLevelBuildDesc.ScratchAccelerationStructureData = pScratchResource->GetGPUVirtualAddress();
		bottomLevelBuildDesc.DestAccelerationStructureData = m_pBottomLevelAccelerationStructure->GetGPUVirtualAddress();

		pRaytracingCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
		pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_pBottomLevelAccelerationStructure));
	}

	DeferredDeletionUniquePtr<ID3D12Resource> pInstanceDesc(&context);
	{
		D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC instanceDesc = {};
		instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
		instanceDesc.InstanceMask = 1;
		instanceDesc.AccelerationStructure = bottomLevelPointer;
		context.UploadData(&instanceDesc, sizeof(instanceDesc), &pInstanceDesc);
	}

	// Top Level Acceleration Structure desc
	D3D12BufferDescriptor topLevelDescriptor;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	{
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS &topLevelInputs = topLevelBuildDesc.Inputs;
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		topLevelInputs.Flags = buildFlags;
		topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		topLevelInputs.NumDescs = 1;
		topLevelInputs.pGeometryDescs = nullptr;
		topLevelInputs.InstanceDescs = pInstanceDesc->GetGPUVirtualAddress();

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
		raytracingDevice.GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);

		DeferredDeletionUniquePtr<ID3D12Resource> pScratchResource(&context);
		AllocateUAVBuffer(
			device,
			topLevelPrebuildInfo.ScratchDataSizeInBytes,
			&pScratchResource,
			L"AS Build Scratch Buffer");

		topLevelDescriptor = context.GetDescriptorAllocator().AllocateAccelerationStructureBuffer(
			topLevelPrebuildInfo.ResultDataMaxSizeInBytes,
			&m_pTopLevelAccelerationStructure);

		topLevelBuildDesc.DestAccelerationStructureData = m_pTopLevelAccelerationStructure->GetGPUVirtualAddress();
		topLevelInputs.InstanceDescs = pInstanceDesc->GetGPUVirtualAddress();
		topLevelBuildDesc.ScratchAccelerationStructureData = pScratchResource->GetGPUVirtualAddress();

		pRaytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
		pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_pTopLevelAccelerationStructure));
	}

	pCommandList->Close();
	context.ExecuteCommandList(pCommandList);

	return topLevelDescriptor;
}
