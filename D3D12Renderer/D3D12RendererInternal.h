#pragma once

class D3D12Renderer : public Renderer
{
public:
    D3D12Renderer(ID3D12CommandQueue *pCommandQueue);

	virtual void SetCanvas(std::shared_ptr<Canvas> pCanvas);

	virtual void DrawScene(Camera &camera, Scene &scene, const RenderSettings &RenderFlags);

	virtual std::shared_ptr<Geometry> CreateGeometry(_In_ CreateGeometryDescriptor &desc);

	virtual std::shared_ptr<Light> CreateLight(_In_ CreateLightDescriptor &desc);

	virtual std::shared_ptr<Camera> CreateCamera(_In_ CreateCameraDescriptor &desc);

	virtual std::shared_ptr<Material> CreateMaterial(_In_ CreateMaterialDescriptor &desc);

	virtual  std::shared_ptr<EnvironmentMap> CreateEnvironmentMap(CreateEnvironmentMapDescriptor &desc);

	virtual std::shared_ptr<Scene> CreateScene(std::shared_ptr<EnvironmentMap> pEnvironmentMap);

private:
	void InitializeStateObject();
	void InitializeRootSignature();

	std::shared_ptr<D3D12Canvas> m_pCanvas = nullptr;

	CComPtr<ID3D12RaytracingFallbackStateObject> m_pStateObject;

	enum
	{
		GlobalRSAccelerationStructureSlot = 0,
		GlobalRSOutputUAVSlot,
		GlobalRSConstantsSlot,
		GlobalRSEnvironmentMapSlot,
		GlobalRSNumSlots,
	};

	enum
	{
		LocalDescriptorTableIndexBufferIndex = 0,
		LocalDescriptorTableAttributeBufferIndex,
		LocalDescriptorTableSize,
	};

	enum
	{
		LocalRSDescriptorTableSlot = 0,
		LocalRSNumSlots,
	};

	CComPtr<ID3D12RootSignature> m_pGlobalRootSignature;
	CComPtr<ID3D12RootSignature> m_pLocalRootSignature;
	
	CComPtr<ID3D12Resource> m_pOutputUAV;
	CComPtr<ID3D12Resource> m_pHitGroupShaderTable;
	CComPtr<ID3D12Resource> m_pMissShaderTable;
	CComPtr<ID3D12Resource> m_pRaygenShaderTable;
	D3D12Descriptor m_OutputUAVDescriptor;

 	D3D12Context m_Context;
};