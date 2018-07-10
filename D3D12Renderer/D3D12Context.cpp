#include "stdafx.h"

D3D12Context::D3D12Context(ID3D12CommandQueue *pQueue, UINT NodeMask) : 
	m_NodeMask(NodeMask),
	m_pQueue(pQueue),
	m_lastFenceValue(0)
{
	ThrowIfFailed(m_pQueue->GetDevice(IID_PPV_ARGS(&m_pDevice)));
	ThrowIfFailed(D3D12CreateRaytracingFallbackDevice(
		m_pDevice, 
		CreateRaytracingFallbackDeviceFlags::None, 
		NodeMask, 
		IID_PPV_ARGS(&m_pRaytracingFallbackDevice)));
	
	m_pDescriptorAllocator = std::make_unique<D3D12DescriptorAllocator>(m_pRaytracingFallbackDevice, NodeMask);
	m_pDevice->CreateFence(m_lastFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence));

}

void D3D12Context::UploadData(void *pData, UINT64 dataSize, ID3D12Resource **ppBuffer, bool bIsConstantBuffer)
{
	UINT allocationSize = bIsConstantBuffer ? max(dataSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) : dataSize;

	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(allocationSize);
	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	ThrowIfFailed(m_pDevice->CreateCommittedResource(
		&heapProperties, 
		D3D12_HEAP_FLAG_NONE, 
		&bufferDesc, 
		D3D12_RESOURCE_STATE_GENERIC_READ, 
		nullptr, 
		IID_PPV_ARGS(ppBuffer)));

	void *pMappedData;
	ThrowIfFailed((*ppBuffer)->Map(0, nullptr, &pMappedData));
	memcpy(pMappedData, pData, dataSize);
	(*ppBuffer)->Unmap(0, nullptr);
}

D3D12Context::CommandListAllocatorPair D3D12Context::CreateCommandListAllocatorPair()
{
	CommandListAllocatorPair pair;
	const auto commandListType = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ThrowIfFailed(m_pDevice->CreateCommandAllocator(commandListType, IID_PPV_ARGS(&pair.m_pAllocator)));
	ThrowIfFailed(m_pDevice->CreateCommandList(m_NodeMask, commandListType, pair.m_pAllocator, nullptr, IID_PPV_ARGS(&pair.m_pCommandList)));
	return pair;
}

D3D12Context::RaytracingCommandListAllocatorPair D3D12Context::CreateRaytracingCommandListAllocatorPair()
{
	auto commandListAllocatorPair = CreateCommandListAllocatorPair();
	
	RaytracingCommandListAllocatorPair raytracingPair;
	GetRaytracingDevice().QueryRaytracingCommandList(commandListAllocatorPair.m_pCommandList, IID_PPV_ARGS(&raytracingPair.m_pCommandList));
	raytracingPair.m_pAllocator = commandListAllocatorPair.m_pAllocator;
	return raytracingPair;
}

void D3D12Context::ExecuteCommandList(ID3D12GraphicsCommandList *pCommandList)
{
	ID3D12CommandList *CommandLists[] = { pCommandList };
	m_pQueue->ExecuteCommandLists(ARRAYSIZE(CommandLists), CommandLists);
	m_pQueue->Signal(m_pFence, ++m_lastFenceValue);
}

void D3D12Context::ClearDeferDeletionQueue()
{
	UINT completedFence = m_pFence->GetCompletedValue();
	while (m_DeferredDeletionQueue.size() && m_DeferredDeletionQueue.front().second <= completedFence)
	{

		m_DeferredDeletionQueue.pop_front();
	}
}

