#pragma once
class D3D12Descriptor
{
public:
	D3D12Descriptor()  {}

	D3D12Descriptor(ID3D12Device *pDevice, UINT DescriptorIndex, ID3D12DescriptorHeap *pHeap) :
		m_Index(DescriptorIndex)
	{
		m_DescriptorSize = pDevice->GetDescriptorHandleIncrementSize(pHeap->GetDesc().Type);
		m_GpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
			pHeap->GetGPUDescriptorHandleForHeapStart(),
			m_Index,
			m_DescriptorSize);
		m_CpuDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
			pHeap->GetCPUDescriptorHandleForHeapStart(),
			m_Index,
			m_DescriptorSize);
	}

	D3D12Descriptor& operator+=(const int index) {

		m_Index += index;
		m_GpuDescriptorHandle = m_GpuDescriptorHandle.Offset(index, m_DescriptorSize);
		m_CpuDescriptorHandle = m_CpuDescriptorHandle.Offset(index, m_DescriptorSize);
		return *this;
	}

	UINT Index() { return m_Index; }
	operator D3D12_GPU_DESCRIPTOR_HANDLE() const
	{
		return m_GpuDescriptorHandle;
	}

	operator D3D12_CPU_DESCRIPTOR_HANDLE() const
	{
		return m_CpuDescriptorHandle;
	}

private:
	UINT m_Index = 0;
	UINT m_DescriptorSize = 0;
	CD3DX12_GPU_DESCRIPTOR_HANDLE m_GpuDescriptorHandle = {};
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_CpuDescriptorHandle = {};
};

static CComPtr<ID3D12Device> GetDevice(ID3D12RaytracingFallbackDevice *pFallbackDevice)
{
	CComPtr<ID3D12Device> pDevice;
	pFallbackDevice->QueryInterface(&pDevice);
	return pDevice;
}

class D3D12BufferDescriptor : public D3D12Descriptor
{
public:
	D3D12BufferDescriptor() {}
	D3D12BufferDescriptor(
		ID3D12RaytracingFallbackDevice *pDevice, 
		UINT DescriptorIndex, 
		ID3D12DescriptorHeap *pHeap, 
		ID3D12Resource *pResource) : D3D12Descriptor(GetDevice(pDevice), DescriptorIndex, pHeap)
	{
		assert(pResource->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER);
		m_WrappedGpuPointer = pDevice->GetWrappedPointerSimple(DescriptorIndex, pResource->GetGPUVirtualAddress());
	}

	operator WRAPPED_GPU_POINTER() const
	{
		return m_WrappedGpuPointer;
	}
private:
	WRAPPED_GPU_POINTER m_WrappedGpuPointer = {};
};

class D3D12DescriptorAllocator
{
public:
	D3D12DescriptorAllocator(ID3D12RaytracingFallbackDevice *pDevice, UINT NodeMask);

	D3D12BufferDescriptor AllocateRawUAVBuffer(UINT64 size, ID3D12Resource **ppOutputResource);
	D3D12BufferDescriptor AllocateAccelerationStructureBuffer(UINT64 size, ID3D12Resource **ppOutputResource);
	D3D12Descriptor AllocateDescriptor(UINT NumContiguousDesciptors = 1);

	void DeleteDescriptor(D3D12Descriptor &descriptor);

	ID3D12DescriptorHeap &GetDescriptorHeap() { return *m_pDescriptorHeap; }
private:
	D3D12BufferDescriptor AllocateUAVBufferInternal(UINT64 size, D3D12_RESOURCE_STATES initialState, ID3D12Resource **ppOutputResource);

	const UINT cDescriptorHeapSize = 100000;
	const UINT cMaxDescriptorTableSize = 4;
	
	std::deque<UINT> m_FreeDescriptorSlotList;
	CComPtr<ID3D12RaytracingFallbackDevice> m_pRaytracingDevice;
	CComPtr<ID3D12Device> m_pDevice;
	CComPtr<ID3D12DescriptorHeap> m_pDescriptorHeap;
};