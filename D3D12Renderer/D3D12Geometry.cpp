#include "stdafx.h"

D3D12Geometry::D3D12Geometry(D3D12Context &context, CreateGeometryDescriptor &desc)
{
	m_pMaterial = safe_dynamic_cast<D3D12Material>(desc.m_pMaterial);
	
	std::vector<Vec3> vertices;
	std::vector<VertexAttribute> attributes;
	for (UINT vertexIndex = 0; vertexIndex < desc.m_NumVertices; vertexIndex++)
	{
		auto &vertex = desc.m_pVertices[vertexIndex];
		vertices.push_back(vertex.m_Position);

		VertexAttribute attribute;
		attribute.Normal = { vertex.m_Normal.x, vertex.m_Normal.y, vertex.m_Normal.z };
		attribute.Tangent = { vertex.m_Tangent.x, vertex.m_Tangent.y, vertex.m_Tangent.z };
		attribute.UV = { vertex.m_Tex.x, vertex.m_Tex.y };
		attributes.push_back(attribute);
	}

	context.UploadData(attributes.data(), attributes.size() * sizeof(attributes[0]), &m_pAttributeBuffer);
	context.UploadData(vertices.data(), vertices.size() * sizeof(vertices[0]), &m_pVertexBuffer);
	if (desc.m_pIndices)
	{
		context.UploadData(desc.m_pIndices, desc.m_NumIndices * sizeof(desc.m_pIndices[0]), &m_pIndexBuffer);
	}

	m_DescriptorTable = context.GetDescriptorAllocator().AllocateDescriptor(GeometryDescriptorTableSize);
	{
		auto indexBufferDescriptor = m_DescriptorTable + GeometryDescriptorTableIndexBufferSlot;
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_UINT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.NumElements = desc.m_NumIndices;
		context.GetDevice().CreateShaderResourceView(m_pIndexBuffer, &srvDesc, indexBufferDescriptor);
	}

	{
		auto attributeBufferDescriptor = m_DescriptorTable + GeometryDescriptorTableAttributeBufferSlot;
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
		srvDesc.Buffer.NumElements = desc.m_NumVertices * sizeof(VertexAttribute) / 4;
		context.GetDevice().CreateShaderResourceView(m_pAttributeBuffer, &srvDesc, attributeBufferDescriptor);
	}
}

D3D12_RAYTRACING_GEOMETRY_DESC D3D12Geometry::GetGeometryDesc()
{
	D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
	geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometryDesc.Triangles.IndexBuffer = m_pIndexBuffer->GetGPUVirtualAddress();
	geometryDesc.Triangles.IndexCount = static_cast<UINT>(m_pIndexBuffer->GetDesc().Width) / (sizeof(UINT32));
	geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
	geometryDesc.Triangles.Transform3x4 = 0;
	geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geometryDesc.Triangles.VertexCount = static_cast<UINT>(m_pVertexBuffer->GetDesc().Width) / (sizeof(float) * 3);
	geometryDesc.Triangles.VertexBuffer.StartAddress = m_pVertexBuffer->GetGPUVirtualAddress();
	geometryDesc.Triangles.VertexBuffer.StrideInBytes = (sizeof(float) * 3);
	geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	return geometryDesc;
}
