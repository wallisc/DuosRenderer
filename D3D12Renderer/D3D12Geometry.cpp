#include "stdafx.h"

D3D12Geometry::D3D12Geometry(D3D12Context &context, CreateGeometryDescriptor &desc) :
	m_pMaterial(desc.m_pMaterial)
{
	std::vector<Vec3> vertices;
	for (UINT vertexIndex = 0; vertexIndex < desc.m_NumVertices; vertexIndex++)
	{
		auto &vertex = desc.m_pVertices[vertexIndex];
		vertices.push_back(vertex.m_Position);
	}

	context.UploadData(vertices.data(), vertices.size() * sizeof(vertices[0]), &m_pVertexBuffer);
	if (desc.m_pIndices)
	{
		context.UploadData(desc.m_pIndices, desc.m_NumIndices * sizeof(desc.m_pIndices[0]), &m_pIndexBuffer);
	}
}

D3D12_RAYTRACING_GEOMETRY_DESC D3D12Geometry::GetGeometryDesc()
{
	D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
	geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometryDesc.Triangles.IndexBuffer = m_pIndexBuffer->GetGPUVirtualAddress();
	geometryDesc.Triangles.IndexCount = static_cast<UINT>(m_pIndexBuffer->GetDesc().Width) / (sizeof(UINT32));
	geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
	geometryDesc.Triangles.Transform = 0;
	geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geometryDesc.Triangles.VertexCount = static_cast<UINT>(m_pVertexBuffer->GetDesc().Width) / (sizeof(float) * 3);
	geometryDesc.Triangles.VertexBuffer.StartAddress = m_pVertexBuffer->GetGPUVirtualAddress();
	geometryDesc.Triangles.VertexBuffer.StrideInBytes = (sizeof(float) * 3);
	geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	return geometryDesc;
}


void D3D12Geometry::BuildAccelerationStructure(D3D12Context &context)
{
	DeferDeletedCommandListAllocatorPair commandListAllocatorPair(context, context.CreateCommandListAllocatorPair());
	auto pCommandList = commandListAllocatorPair.m_pCommandList;
	auto &raytracingDevice = context.GetRaytracingDevice();
	auto &device = context.GetDevice();
	CComPtr<ID3D12RaytracingFallbackCommandList> pRaytracingCommandList;
	raytracingDevice.QueryRaytracingCommandList(pCommandList, IID_PPV_ARGS(&pRaytracingCommandList));


	D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
	geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometryDesc.Triangles.IndexBuffer = m_pIndexBuffer->GetGPUVirtualAddress();
	geometryDesc.Triangles.IndexCount = static_cast<UINT>(m_pIndexBuffer->GetDesc().Width) / (sizeof(UINT32));
	geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
	geometryDesc.Triangles.Transform = 0;
	geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geometryDesc.Triangles.VertexCount = static_cast<UINT>(m_pVertexBuffer->GetDesc().Width) / (sizeof(float) * 3);
	geometryDesc.Triangles.VertexBuffer.StartAddress = m_pVertexBuffer->GetGPUVirtualAddress();
	geometryDesc.Triangles.VertexBuffer.StrideInBytes = (sizeof(float) * 3);
	geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	// Get required sizes for an acceleration structure.
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	D3D12_GET_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO_DESC prebuildInfoDesc = {};
	prebuildInfoDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	prebuildInfoDesc.Flags = buildFlags;
	prebuildInfoDesc.NumDescs = 1;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
	prebuildInfoDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	prebuildInfoDesc.pGeometryDescs = &geometryDesc;
	raytracingDevice.GetRaytracingAccelerationStructurePrebuildInfo(&prebuildInfoDesc, &bottomLevelPrebuildInfo);
	assert(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

	DeferredDeletionUniquePtr<ID3D12Resource> pScratchResource(&context);
	AllocateUAVBuffer(
		device, 
		bottomLevelPrebuildInfo.ScratchDataSizeInBytes,
		&pScratchResource, 
		L"AS Build Scratch Buffer");

	m_BottomLevelAccelerationStructurePointer = context.GetDescriptorAllocator().AllocateAccelerationStructureBuffer(
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
		bottomLevelBuildDesc.NumDescs = 1;
		bottomLevelBuildDesc.pGeometryDescs = &geometryDesc;
	}

	pRaytracingCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc);
	pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_pBottomLevelAccelerationStructure));
	pCommandList->Close();
	context.ExecuteCommandList(pCommandList);
}
