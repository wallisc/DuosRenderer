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

	D3D12Context &GetContext() { return m_Context; }
	ID3D12RaytracingFallbackStateObject &GetStateObject() { return *m_pStateObject; }
private:
	void InitializeStateObject();
	void InitializeRootSignature();

	std::shared_ptr<D3D12Canvas> m_pCanvas = nullptr;

	CComPtr<ID3D12RaytracingFallbackStateObject> m_pStateObject;

	class ShaderIdentifierOnlyShaderRecord
	{
		byte m_ShaderIdentifier[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES];
	};

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
		LocalRSGeometryDescriptorTableSlot = 0,
		LocalRSMaterialDescriptorTableSlot = 1,
		LocalRSNumSlots,
	};

	CComPtr<ID3D12RootSignature> m_pGlobalRootSignature;
	CComPtr<ID3D12RootSignature> m_pLocalRootSignature;
	
	CComPtr<ID3D12Resource> m_pAveragedOutputUAV;
	CComPtr<ID3D12Resource> m_pAccumulatedOutputUAV;
	CComPtr<ID3D12Resource> m_pMissShaderTable;
	CComPtr<ID3D12Resource> m_pRaygenShaderTable;
	D3D12Descriptor m_AccumulatedOutputUAVDescriptor;
	D3D12Descriptor m_AveragedOutputUAVDescriptor;

	UINT m_SampleCount;

 	D3D12Context m_Context;
	AverageSamplesPass m_AverageSamplesPass;
};

static const auto HitGroupExportName = L"MyHitGroup";
static const auto LightingMissShaderExportName = L"LightingMiss";
static const auto OcclusionMissShaderExportName = L"OcclusionMiss";
