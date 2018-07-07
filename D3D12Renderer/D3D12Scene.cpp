#include "stdafx.h"

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
		D3D12_GET_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO_DESC prebuildInfoDesc = {};
		prebuildInfoDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		prebuildInfoDesc.Flags = buildFlags;
		prebuildInfoDesc.NumDescs = geometryDescs.size()  ;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
		prebuildInfoDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		prebuildInfoDesc.pGeometryDescs = geometryDescs.data();
		raytracingDevice.GetRaytracingAccelerationStructurePrebuildInfo(&prebuildInfoDesc, &bottomLevelPrebuildInfo);
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
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
		{
			bottomLevelBuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			bottomLevelBuildDesc.Flags = buildFlags;
			bottomLevelBuildDesc.ScratchAccelerationStructureData = { pScratchResource->GetGPUVirtualAddress(), pScratchResource->GetDesc().Width };
			bottomLevelBuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
			bottomLevelBuildDesc.DestAccelerationStructureData = { m_pBottomLevelAccelerationStructure->GetGPUVirtualAddress(), bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes };
			bottomLevelBuildDesc.NumDescs = geometryDescs.size();
			bottomLevelBuildDesc.pGeometryDescs = geometryDescs.data();
		}

		pRaytracingCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc);
		pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_pBottomLevelAccelerationStructure));
	}

	DeferredDeletionUniquePtr<ID3D12Resource> pInstanceDesc(&context);
	{
		D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC instanceDesc = {};
		instanceDesc.Transform[0] = instanceDesc.Transform[5] = instanceDesc.Transform[10] = 1;
		instanceDesc.InstanceMask = 1;
		instanceDesc.AccelerationStructure = bottomLevelPointer;
		context.UploadData(&instanceDesc, sizeof(instanceDesc), &pInstanceDesc);
	}

	// Top Level Acceleration Structure desc
	D3D12BufferDescriptor topLevelDescriptor;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	{
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		D3D12_GET_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO_DESC prebuildInfoDesc = {};
		prebuildInfoDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		prebuildInfoDesc.Flags = buildFlags;
		prebuildInfoDesc.NumDescs = 1;
		prebuildInfoDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
		raytracingDevice.GetRaytracingAccelerationStructurePrebuildInfo(&prebuildInfoDesc, &topLevelPrebuildInfo);

		DeferredDeletionUniquePtr<ID3D12Resource> pScratchResource(&context);
		AllocateUAVBuffer(
			device,
			topLevelPrebuildInfo.ScratchDataSizeInBytes,
			&pScratchResource,
			L"AS Build Scratch Buffer");

		topLevelDescriptor = context.GetDescriptorAllocator().AllocateAccelerationStructureBuffer(
			topLevelPrebuildInfo.ResultDataMaxSizeInBytes,
			&m_pTopLevelAccelerationStructure);

		topLevelBuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		topLevelBuildDesc.Flags = buildFlags;
		topLevelBuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		topLevelBuildDesc.DestAccelerationStructureData = { m_pTopLevelAccelerationStructure->GetGPUVirtualAddress(), topLevelPrebuildInfo.ResultDataMaxSizeInBytes };
		topLevelBuildDesc.NumDescs = 1;
		topLevelBuildDesc.pGeometryDescs = nullptr;
		topLevelBuildDesc.InstanceDescs = pInstanceDesc->GetGPUVirtualAddress();
		topLevelBuildDesc.ScratchAccelerationStructureData = { pScratchResource->GetGPUVirtualAddress(), pScratchResource->GetDesc().Width };

		pRaytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc);
		pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_pTopLevelAccelerationStructure));
	}

	pCommandList->Close();
	context.ExecuteCommandList(pCommandList);

	return topLevelDescriptor;
}
