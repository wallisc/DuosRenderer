#pragma once

static void AllocateGpuWriteBuffer(ID3D12Device &device, UINT64 bufferSize, ID3D12Resource **ppOutputResource, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS, LPCWSTR pName = nullptr)
{
	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device.CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		initialState,
		nullptr,
		IID_PPV_ARGS(ppOutputResource)));

	if (pName)
	{
		(*ppOutputResource)->SetName(pName);
	}
}

static void AllocateUAVBuffer(ID3D12Device &device, UINT64 bufferSize, ID3D12Resource **ppOutputResource, LPCWSTR pName = nullptr)
{
	AllocateGpuWriteBuffer(device, bufferSize, ppOutputResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, pName);
}

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

class ScopedResourceBarrier
{
public:
	ScopedResourceBarrier(ID3D12GraphicsCommandList *pCommandList, ID3D12Resource *pResource, D3D12_RESOURCE_STATES OriginalState, D3D12_RESOURCE_STATES TempState) :
		m_pCommandList(pCommandList), m_pResource(pResource), m_OriginalState(OriginalState), m_TempState(TempState)
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pResource, m_OriginalState, m_TempState);
		m_pCommandList->ResourceBarrier(1, &barrier);
	}

	~ScopedResourceBarrier()
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pResource, m_TempState, m_OriginalState);
		m_pCommandList->ResourceBarrier(1, &barrier);
	}
private:
	CComPtr<ID3D12GraphicsCommandList> m_pCommandList;
	CComPtr<ID3D12Resource> m_pResource;
	D3D12_RESOURCE_STATES m_OriginalState, m_TempState;
};