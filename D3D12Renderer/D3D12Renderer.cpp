#include "stdafx.h"
#include "D3D12RaytracingHelpers.hpp"
#include "CompiledShaders\Raytracing.h"

using namespace std;

shared_ptr<Renderer> CreateD3D12Renderer(ID3D12CommandQueue *pQueue)
{
	return make_shared<D3D12Renderer>(pQueue);
}

D3D12Renderer::D3D12Renderer(ID3D12CommandQueue *pQueue) :
	m_Context(pQueue)
{
	// Initialize the windows runtime for dxtk12
	CoInitializeEx(nullptr, COINIT_SPEED_OVER_MEMORY | COINIT_MULTITHREADED);

	InitializeRootSignature();
	InitializeStateObject();
}

void D3D12Renderer::SetCanvas(std::shared_ptr<Canvas> pCanvas)
{
	auto &descriptorAllocator = m_Context.GetDescriptorAllocator();
	bool bAllocateNewUAV = true;

	m_pCanvas = dynamic_pointer_cast<D3D12Canvas>(pCanvas);
	if (m_pOutputUAV)
	{
		auto &uavDesc = m_pOutputUAV->GetDesc();
		auto &canvasDesc = m_pCanvas->GetCanvasResource().GetDesc();
		if (uavDesc.Width == canvasDesc.Width &&
			uavDesc.Height == canvasDesc.Height)
		{
			bAllocateNewUAV = false;
		}
		else
		{
			descriptorAllocator.DeleteDescriptor(m_OutputUAVDescriptor);
			m_pOutputUAV = nullptr;
		}
	}

	if (bAllocateNewUAV)
	{
		auto &renderTarget = m_pCanvas->GetCanvasResource();
		auto renderTargetDesc = renderTarget.GetDesc();

		auto uavResourceDesc = renderTargetDesc;
		uavResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		ThrowIfFailed(m_Context.GetDevice().CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&uavResourceDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&m_pOutputUAV)));

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = uavResourceDesc.Format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		m_OutputUAVDescriptor = descriptorAllocator.AllocateDescriptor();
		m_Context.GetDevice().CreateUnorderedAccessView(
			m_pOutputUAV,
			nullptr,
			&uavDesc,
			m_OutputUAVDescriptor);
	}
}

void D3D12Renderer::DrawScene(Camera &camera, Scene &scene, const RenderSettings &RenderFlags)
{
	auto pD3D12Scene = dynamic_cast<D3D12Scene*>(&scene);
	auto pD3D12Camera = dynamic_cast<D3D12Camera*>(&camera);
	
	auto &context = m_Context;
	DeferDeletedCommandListAllocatorPair commandListAllocatorPair(context, context.CreateCommandListAllocatorPair());
	auto pCommandList = commandListAllocatorPair.m_pCommandList;
	auto &raytracingDevice = context.GetRaytracingDevice();
	auto &device = context.GetDevice();
	CComPtr<ID3D12RaytracingFallbackCommandList> pRaytracingCommandList;
	raytracingDevice.QueryRaytracingCommandList(pCommandList, IID_PPV_ARGS(&pRaytracingCommandList));

	pCommandList->SetComputeRootSignature(m_pGlobalRootSignature);
	ID3D12DescriptorHeap *pHeaps[] = { &context.GetDescriptorAllocator().GetDescriptorHeap() };
	pRaytracingCommandList->SetDescriptorHeaps(ARRAYSIZE(pHeaps), pHeaps);
	auto &outputResourceDesc = m_pOutputUAV->GetDesc();

	auto &sceneConstants = pD3D12Camera->GetConstantData();;

	pRaytracingCommandList->SetTopLevelAccelerationStructure(GlobalRSAccelerationStructureSlot, pD3D12Scene->GetTopLevelAccelerationStructure());
	pCommandList->SetComputeRootDescriptorTable(GlobalRSOutputUAVSlot, m_OutputUAVDescriptor);
	pCommandList->SetComputeRootDescriptorTable(GlobalRSEnvironmentMapSlot, pD3D12Scene->GetEnvironmentMap());
	pCommandList->SetComputeRoot32BitConstants(GlobalRSConstantsSlot, SizeOfInUint32(sceneConstants), &sceneConstants, 0);

	D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = {};
	dispatchRaysDesc.HitGroupTable = pD3D12Scene->GetHitGroupTable();
	dispatchRaysDesc.MissShaderTable.StartAddress = m_pMissShaderTable->GetGPUVirtualAddress();
	dispatchRaysDesc.MissShaderTable.SizeInBytes = m_pMissShaderTable->GetDesc().Width;
	dispatchRaysDesc.RayGenerationShaderRecord.StartAddress = m_pRaygenShaderTable->GetGPUVirtualAddress();
	dispatchRaysDesc.RayGenerationShaderRecord.SizeInBytes = m_pRaygenShaderTable->GetDesc().Width;
	
	dispatchRaysDesc.Height = outputResourceDesc.Height;
	dispatchRaysDesc.Width = static_cast<UINT>(outputResourceDesc.Width);
	dispatchRaysDesc.Depth = 1;
	pRaytracingCommandList->SetPipelineState1(m_pStateObject);
	pRaytracingCommandList->DispatchRays(&dispatchRaysDesc);

	{
		pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_pOutputUAV));
		ScopedResourceBarrier rtvBarrier(pCommandList, &m_pCanvas->GetCanvasResource(), m_pCanvas->GetResourceSate(), D3D12_RESOURCE_STATE_COPY_DEST);
		ScopedResourceBarrier uavBarrier(pCommandList, m_pOutputUAV, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

		pCommandList->CopyResource(&m_pCanvas->GetCanvasResource(), m_pOutputUAV);
	}

	pCommandList->Close();
	context.ExecuteCommandList(pCommandList);
}

shared_ptr<Geometry> D3D12Renderer::CreateGeometry(_In_ CreateGeometryDescriptor &desc)
{
	return make_shared<D3D12Geometry>(m_Context, desc);
}

shared_ptr<Light> D3D12Renderer::CreateLight(_In_ CreateLightDescriptor &desc)
{
	return make_shared<D3D12Light>(desc);
}

shared_ptr<Camera> D3D12Renderer::CreateCamera(_In_ CreateCameraDescriptor &desc)
{
	return make_shared<D3D12Camera>(desc);
}

shared_ptr<Material> D3D12Renderer::CreateMaterial(_In_ CreateMaterialDescriptor &desc)
{
	return make_shared<D3D12Material>(desc);
}

shared_ptr<EnvironmentMap> D3D12Renderer::CreateEnvironmentMap(CreateEnvironmentMapDescriptor &desc)
{
	return make_shared<D3D12EnvironmentMap>(m_Context, desc);
}

shared_ptr<Scene> D3D12Renderer::CreateScene(shared_ptr<EnvironmentMap> pEnvironmentMap)
{
	return make_shared<D3D12Scene>(m_Context, GetStateObject(), pEnvironmentMap);
}

void D3D12Renderer::InitializeRootSignature()
{
	auto &device = m_Context.GetRaytracingDevice();
	{
		auto uavRange = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
		auto environmentMapRange = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, EnvironmentMapSRVRegister);

		CD3DX12_ROOT_PARAMETER1 globalRootParameters[GlobalRSNumSlots];
		globalRootParameters[GlobalRSAccelerationStructureSlot].InitAsShaderResourceView(0);
		globalRootParameters[GlobalRSOutputUAVSlot].InitAsDescriptorTable(1, &uavRange);
		globalRootParameters[GlobalRSEnvironmentMapSlot].InitAsDescriptorTable(1, &environmentMapRange);
		globalRootParameters[GlobalRSConstantsSlot].InitAsConstants(20, 0);

		auto staticSampler = CD3DX12_STATIC_SAMPLER_DESC(LinearSamplerRegister, D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR);

		auto globalRSDesc = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC(
			ARRAYSIZE(globalRootParameters), globalRootParameters,
			1, &staticSampler);

		CComPtr<ID3DBlob> pSerializedBlob;
		ThrowIfFailed(device.D3D12SerializeVersionedRootSignature(&globalRSDesc, &pSerializedBlob, nullptr));
		ThrowIfFailed(device.CreateRootSignature(
			m_Context.GetNodeMask(),
			pSerializedBlob->GetBufferPointer(),
			pSerializedBlob->GetBufferSize(),
			IID_PPV_ARGS(&m_pGlobalRootSignature)));
	}

	{
		CD3DX12_DESCRIPTOR_RANGE1 descriptorTable[D3D12Geometry::GeometryDescriptorTableSize];
		descriptorTable[D3D12Geometry::GeometryDescriptorTableIndexBufferSlot].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, IndexBufferSRVRegister, LocalRootSignatureRegisterSpace);
		descriptorTable[D3D12Geometry::GeometryDescriptorTableAttributeBufferSlot].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, AttributeBufferSRVRegister, LocalRootSignatureRegisterSpace);

		CD3DX12_ROOT_PARAMETER1 localRootParameters[LocalRSNumSlots];
		localRootParameters[LocalRSDescriptorTableSlot].InitAsDescriptorTable(ARRAYSIZE(descriptorTable), descriptorTable);

		auto localRSDesc = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC(ARRAYSIZE(localRootParameters), localRootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

		CComPtr<ID3DBlob> pSerializedBlob;
		ThrowIfFailed(device.D3D12SerializeVersionedRootSignature(&localRSDesc, &pSerializedBlob, nullptr));
		ThrowIfFailed(device.CreateRootSignature(
			m_Context.GetNodeMask(),
			pSerializedBlob->GetBufferPointer(),
			pSerializedBlob->GetBufferSize(),
			IID_PPV_ARGS(&m_pLocalRootSignature)));
	}
}

void D3D12Renderer::InitializeStateObject()
{
	auto MissExportName = L"MyMissShader";
	auto RaygenExportName = L"MyRaygenShader";

	CD3D12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	// DXIL library
	// This contains the shaders and their entrypoints for the state object.
	// Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.
	auto lib = raytracingPipeline.CreateSubobject<CD3D12_DXIL_LIBRARY_SUBOBJECT>();
	D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void *)g_pRaytracing, ARRAYSIZE(g_pRaytracing));
	lib->SetDXILLibrary(&libdxil);
	// Define which shader exports to surface from the library.
	// If no shader exports are defined for a DXIL library subobject, all shaders will be surfaced.
	// In this sample, this could be ommited for convenience since the sample uses all shaders in the library. 
	{
		lib->DefineExport(RaygenExportName);
		lib->DefineExport(L"MyClosestHitShader");
		lib->DefineExport(MissExportName);
	}

	// Triangle hit group
	// A hit group specifies closest hit, any hit and intersection shaders to be executed when a ray intersects the geometry's triangle/AABB.
	// In this sample, we only use triangle geometry with a closest hit shader, so others are not set.
	auto hitGroup = raytracingPipeline.CreateSubobject<CD3D12_HIT_GROUP_SUBOBJECT>();
	hitGroup->SetClosestHitShaderImport(L"MyClosestHitShader");
	hitGroup->SetHitGroupExport(HitGroupExportName);
	
	raytracingPipeline.CreateSubobject<CD3D12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>()->SetRootSignature(m_pGlobalRootSignature);
	raytracingPipeline.CreateSubobject<CD3D12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>()->Config(16, 8);
	raytracingPipeline.CreateSubobject<CD3D12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>()->Config(1);

	auto pHitGroupLocalRootSignature = raytracingPipeline.CreateSubobject<CD3D12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
	pHitGroupLocalRootSignature->SetRootSignature(m_pLocalRootSignature);

	auto pLocalRootSignatureAssociation = raytracingPipeline.CreateSubobject<CD3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
	pLocalRootSignatureAssociation->SetSubobjectToAssociate(*pHitGroupLocalRootSignature);
	pLocalRootSignatureAssociation->AddExport(HitGroupExportName);

	ThrowIfFailed(m_Context.GetRaytracingDevice().CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_pStateObject)));

	auto shaderIdentifierSize = m_Context.GetRaytracingDevice().GetShaderIdentifierSize();
	m_Context.UploadData(m_pStateObject->GetShaderIdentifier(MissExportName), shaderIdentifierSize, &m_pMissShaderTable);
	m_Context.UploadData(m_pStateObject->GetShaderIdentifier(RaygenExportName), shaderIdentifierSize, &m_pRaygenShaderTable);
}

