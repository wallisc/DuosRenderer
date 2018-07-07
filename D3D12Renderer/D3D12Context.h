#pragma once
class D3D12Context
{
public:
	D3D12Context(ID3D12CommandQueue *pQueue, UINT NodeMask = 1);
	void UploadData(void *pData, UINT64 dataSize, ID3D12Resource **ppBuffer);

	ID3D12Device &GetDevice() { return *m_pDevice; }
	ID3D12RaytracingFallbackDevice &GetRaytracingDevice() { return *m_pRaytracingFallbackDevice; }
	ID3D12CommandQueue &GetCommandQueue() { return *m_pQueue; }

	struct CommandListAllocatorPair
	{
		CComPtr<ID3D12GraphicsCommandList> m_pCommandList;
		CComPtr<ID3D12CommandAllocator> m_pAllocator;
	};
	CommandListAllocatorPair CreateCommandListAllocatorPair();

	struct RaytracingCommandListAllocatorPair
	{
		CComPtr<ID3D12RaytracingFallbackCommandList> m_pCommandList;
		CComPtr<ID3D12CommandAllocator> m_pAllocator;
	};
	RaytracingCommandListAllocatorPair CreateRaytracingCommandListAllocatorPair();
	
	D3D12DescriptorAllocator &GetDescriptorAllocator()
	{
		return *m_pDescriptorAllocator;
	}

	void ExecuteCommandList(ID3D12GraphicsCommandList *pCommandList);
	void DeferDelete(IUnknown *pUnknown)
	{
		m_DeferredDeletionQueue.emplace_back(pUnknown, m_lastFenceValue);
	}
	
	UINT GetNodeMask() { return m_NodeMask; }
private:
	std::unique_ptr<D3D12DescriptorAllocator> m_pDescriptorAllocator;
	UINT m_NodeMask;

	CComPtr<ID3D12RaytracingFallbackDevice> m_pRaytracingFallbackDevice;
	CComPtr<ID3D12Device> m_pDevice;
	CComPtr<ID3D12CommandQueue> m_pQueue;

	typedef std::pair <CComPtr<IUnknown>, UINT> ObjectFenceValuePair;
	std::deque<ObjectFenceValuePair> m_DeferredDeletionQueue;
	CComPtr<ID3D12Fence> m_pFence;
	UINT m_lastFenceValue;
};

template<typename Object>
class DeferredDeletionUniquePtr
{
public:
	DeferredDeletionUniquePtr(D3D12Context *pContext = nullptr, Object *object = nullptr) :
		m_pContext(pContext),
		m_pObject(nullptr)
	{
		if (object)
		{
			AddRefAndAttach(object);
		}
	}

	void AddRefAndAttach(Object *object)
	{
		ReleaseObjectToDeletionQueueIfExists();

		m_pObject = object;
		m_pObject->AddRef();
	}

	~DeferredDeletionUniquePtr() {
		assert(m_pContext);
		ReleaseObjectToDeletionQueueIfExists();
	}
	Object *Get() const { return m_pObject; }
	operator Object*() { return m_pObject; }
	Object **operator&() { return &m_pObject; }
	Object *operator->() { return m_pObject; }
private:
	void ReleaseObjectToDeletionQueueIfExists()
	{
		if (m_pObject)
		{
			m_pContext->DeferDelete(m_pObject);

			m_pObject->Release();
			m_pObject = nullptr;
		}
	}

	D3D12Context *m_pContext;
	Object *m_pObject;
};

class DeferDeletedCommandListAllocatorPair
{
public:
	DeferDeletedCommandListAllocatorPair(D3D12Context &context, D3D12Context::CommandListAllocatorPair &pair) :
		m_pCommandList(&context, pair.m_pCommandList),
		m_pAllocator(&context, pair.m_pAllocator)
	{
	}

	DeferredDeletionUniquePtr<ID3D12GraphicsCommandList> m_pCommandList;
	DeferredDeletionUniquePtr<ID3D12CommandAllocator> m_pAllocator;
};