#include "stdafx.h"

D3D12DescriptorAllocator::D3D12DescriptorAllocator(ID3D12RaytracingFallbackDevice *pRaytracingDevice, UINT nodeMask) :
	m_pRaytracingDevice(pRaytracingDevice)
{
	ThrowIfFailed(m_pRaytracingDevice->QueryInterface(&m_pDevice));

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.NodeMask = nodeMask;
	desc.NumDescriptors = cDescriptorHeapSize;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	ThrowIfFailed(m_pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pDescriptorHeap)));

	for (UINT i = 0; i < desc.NumDescriptors; i++)
	{
		m_FreeDescriptorSlotList.push_back(i);
	}
}

D3D12Descriptor D3D12DescriptorAllocator::AllocateDescriptor()
{
	auto descriptorIndex = m_FreeDescriptorSlotList.front();
	m_FreeDescriptorSlotList.pop_front();
	return D3D12Descriptor(m_pDevice, descriptorIndex, m_pDescriptorHeap);
}


D3D12BufferDescriptor D3D12DescriptorAllocator::AllocateUAVBufferInternal(
	UINT64 size,
	D3D12_RESOURCE_STATES initialState, 
	ID3D12Resource **ppOutputResource)
{
	auto descriptor = AllocateDescriptor();
	AllocateGpuWriteBuffer(*m_pDevice, size, ppOutputResource, initialState);

	D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
	desc.Buffer.NumElements = static_cast<UINT>(size / sizeof(UINT32));
	desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
	desc.Format = DXGI_FORMAT_R32_TYPELESS;
	desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	m_pDevice->CreateUnorderedAccessView(*ppOutputResource, nullptr, &desc, descriptor);

	return D3D12BufferDescriptor(m_pRaytracingDevice, descriptor.Index(), m_pDescriptorHeap, *ppOutputResource);
}


D3D12BufferDescriptor D3D12DescriptorAllocator::AllocateRawUAVBuffer(UINT64 size, ID3D12Resource **ppOutputResource)
{
	return AllocateUAVBufferInternal(size, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, ppOutputResource);
}

D3D12BufferDescriptor D3D12DescriptorAllocator::AllocateAccelerationStructureBuffer(UINT64 size, ID3D12Resource **ppOutputResource)
{
	return AllocateUAVBufferInternal(size, m_pRaytracingDevice->GetAccelerationStructureResourceState(), ppOutputResource);
}

void D3D12DescriptorAllocator::DeleteDescriptor(D3D12Descriptor &descriptor)
{
	m_FreeDescriptorSlotList.push_back(descriptor.Index());
}