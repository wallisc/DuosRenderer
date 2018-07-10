//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "stdafx.h"
#include "PBRTViewer.h"
#include "DirectXRaytracingHelper.h"
#include "CompiledShaders\Raytracing.hlsl.h"

#include "D3D12Renderer.h"

#include "PBRTParser.h"
#include <algorithm>


using namespace std;
using namespace DX;

SceneParser::Scene g_scene;
shared_ptr<DuosRenderer::Renderer> g_pRenderer;
shared_ptr<DuosRenderer::Camera> g_pCamera;
shared_ptr<DuosRenderer::Scene> g_pScene;

const wchar_t* PBRTViewer::c_hitGroupName = L"MyHitGroup";
const wchar_t* PBRTViewer::c_raygenShaderName = L"MyRaygenShader";
const wchar_t* PBRTViewer::c_closestHitShaderName = L"MyClosestHitShader";
const wchar_t* PBRTViewer::c_missShaderName = L"MyMissShader";

PBRTViewer::PBRTViewer(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_raytracingOutputResourceUAVDescriptorHeapIndex(UINT_MAX),
    m_curRotationAngleRad(0.0f),
    m_isDxrSupported(false)
{
    m_forceComputeFallback = false;
    UpdateForSizeChange(width, height);
}

void PBRTViewer::EnableDXRExperimentalFeatures(IDXGIAdapter1* adapter)
{
    // DXR is an experimental feature and needs to be enabled before creating a D3D12 device.
    m_isDxrSupported = EnableRaytracing(adapter);

    if (!m_isDxrSupported)
    {
        OutputDebugString(
            L"Could not enable raytracing driver (D3D12EnableExperimentalFeatures() failed).\n" \
            L"Possible reasons:\n" \
            L"  1) your OS is not in developer mode.\n" \
            L"  2) your GPU driver doesn't match the D3D12 runtime loaded by the app (d3d12.dll and friends).\n" \
            L"  3) your D3D12 runtime doesn't match the D3D12 headers used by your app (in particular, the GUID passed to D3D12EnableExperimentalFeatures).\n\n");

        OutputDebugString(L"Enabling compute based fallback raytracing support.\n");
        ThrowIfFalse(EnableComputeRaytracingFallback(adapter), L"Could not enable compute based fallback raytracing support (D3D12EnableExperimentalFeatures() failed).\n");
		m_raytracingAPI = RaytracingAPI::FallbackLayer;
    }
}

void PBRTViewer::OnInit()
{
    m_deviceResources = std::make_unique<DeviceResources>(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_UNKNOWN,
        FrameCount,
        D3D_FEATURE_LEVEL_11_0,
        // Sample shows handling of use cases with tearing support, which is OS dependent and has been supported since TH2.
        // Since the Fallback Layer requires Fall Creator's update (RS3), we don't need to handle non-tearing cases.
        DeviceResources::c_RequireTearingSupport,
        m_adapterIDoverride
        );
    m_deviceResources->RegisterDeviceNotify(this);
    m_deviceResources->SetWindow(Win32Application::GetHwnd(), m_width, m_height);
    m_deviceResources->InitializeDXGIAdapter();
    EnableDXRExperimentalFeatures(m_deviceResources->GetAdapter());

	PBRTParser::PBRTParser().Parse("..\\Assets\\Teapot\\scene.pbrt", g_scene);

    m_deviceResources->CreateDeviceResources();
    m_deviceResources->CreateWindowSizeDependentResources();

    InitializeScene();

    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

// Update camera matrices passed into the shader.
void PBRTViewer::UpdateCameraMatrices()
{
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

	auto &camera = g_scene.m_Camera;
	m_sceneCB[frameIndex].cameraPosition = XMVectorSet(camera.m_Position.x, camera.m_Position.y, camera.m_Position.z, 1.0);
	float fovAngleY = camera.m_FieldOfView;
    XMMATRIX view = XMMatrixLookAtLH(m_eye, m_at, m_up);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), m_aspectRatio, 1.0f, 125.0f);
    XMMATRIX viewProj = view * proj;

    m_sceneCB[frameIndex].projectionToWorld = XMMatrixInverse(nullptr, viewProj);
	m_sceneCB[frameIndex].time = static_cast<UINT>(time(nullptr));
}

// Initialize scene rendering parameters.
void PBRTViewer::InitializeScene()
{
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

    // Setup materials.
    {
        m_cubeCB.albedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // Setup camera.
    {
		auto &camera = g_scene.m_Camera;

        // Initialize the view and projection inverse matrices.
        m_eye = { camera.m_Position.x, camera.m_Position.y, camera.m_Position.z, 1.0f };
        m_at = { camera.m_LookAt.x, camera.m_LookAt.y, camera.m_LookAt.z, 1.0f };
		m_up = { camera.m_Up.x, camera.m_Up.y, camera.m_Up.z, 0.0f };
        
        UpdateCameraMatrices();
    }

    // Apply the initial values to all frames' buffer instances.
    for (auto& sceneCB : m_sceneCB)
    {
        sceneCB = m_sceneCB[frameIndex];
    }
}

// Create constant buffers.
void PBRTViewer::CreateConstantBuffers()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto frameCount = m_deviceResources->GetBackBufferCount();
    
    // Create the constant buffer memory and map the CPU and GPU addresses
    const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // Allocate one constant buffer per frame, since it gets updated every frame.
    size_t cbSize = frameCount * sizeof(AlignedSceneConstantBuffer);
    const D3D12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

    ThrowIfFailed(device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &constantBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_perFrameConstants)));

    // Map the constant buffer and cache its heap pointers.
    // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_perFrameConstants->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedConstantData)));
}

// Create resources that depend on the device.
void PBRTViewer::CreateDeviceDependentResources()
{
    // Initialize raytracing pipeline.

    // Create raytracing interfaces: raytracing device and commandlist.
    CreateRaytracingInterfaces();

    // Create root signatures for the shaders.
    CreateRootSignatures();

    // Build raytracing acceleration structures from the generated geometry.
    BuildAccelerationStructures();

    // Create constant buffers for the geometry and the scene.
    CreateConstantBuffers();
}

void PBRTViewer::SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig)
{
    auto device = m_deviceResources->GetD3DDevice();
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;

    if (m_raytracingAPI == RaytracingAPI::FallbackLayer)
    {
        ThrowIfFailed(m_fallbackDevice->D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
        ThrowIfFailed(m_fallbackDevice->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
    }
    else // DirectX Raytracing
    {
        ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
        ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
    }
}

void PBRTViewer::CreateRootSignatures()
{
    auto device = m_deviceResources->GetD3DDevice();

    // Global Root Signature
    // This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
    {
        CD3DX12_DESCRIPTOR_RANGE ranges[2]; // Perfomance TIP: Order from most frequent to least frequent.
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);  // 1 output texture
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);  // 2 static index and vertex buffers.

        CD3DX12_ROOT_PARAMETER rootParameters[GlobalRootSignatureParams::Count];
        rootParameters[GlobalRootSignatureParams::OutputViewSlot].InitAsDescriptorTable(1, &ranges[0]);
        rootParameters[GlobalRootSignatureParams::AccelerationStructureSlot].InitAsShaderResourceView(0);
        rootParameters[GlobalRootSignatureParams::SceneConstantSlot].InitAsConstantBufferView(0);
        rootParameters[GlobalRootSignatureParams::VertexBuffersSlot].InitAsDescriptorTable(1, &ranges[1]);
        CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
        SerializeAndCreateRaytracingRootSignature(globalRootSignatureDesc, &m_raytracingGlobalRootSignature);
    }

    // Local Root Signature
    // This is a root signature that enables a shader to have unique arguments that come from shader tables.
    {
        CD3DX12_ROOT_PARAMETER rootParameters[LocalRootSignatureParams::Count];
        rootParameters[LocalRootSignatureParams::CubeConstantSlot].InitAsConstants(SizeOfInUint32(m_cubeCB), 1);
        CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
        localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
        SerializeAndCreateRaytracingRootSignature(localRootSignatureDesc, &m_raytracingLocalRootSignature);
    }
#if USE_NON_NULL_LOCAL_ROOT_SIG 
    // Empty local root signature
    {
        CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(D3D12_DEFAULT);
        localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
        SerializeAndCreateRaytracingRootSignature(localRootSignatureDesc, &m_raytracingLocalRootSignatureEmpty);
    }
#endif
}

// Create raytracing device and command list.
void PBRTViewer::CreateRaytracingInterfaces()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto commandList = m_deviceResources->GetCommandList();

    if (m_raytracingAPI == RaytracingAPI::FallbackLayer)
    {
        CreateRaytracingFallbackDeviceFlags createDeviceFlags = m_forceComputeFallback ? 
                                                    CreateRaytracingFallbackDeviceFlags::ForceComputeFallback : 
                                                    CreateRaytracingFallbackDeviceFlags::None;
        ThrowIfFailed(D3D12CreateRaytracingFallbackDevice(device, createDeviceFlags, 0, IID_PPV_ARGS(&m_fallbackDevice)));
        m_fallbackDevice->QueryRaytracingCommandList(commandList, IID_PPV_ARGS(&m_fallbackCommandList));
    }
	else // DirectX Raytracing
	{
		ThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(&m_dxrDevice)), L"Couldn't get DirectX Raytracing interface for the device.\n");
		ThrowIfFailed(commandList->QueryInterface(IID_PPV_ARGS(&m_dxrCommandList)), L"Couldn't get DirectX Raytracing interface for the command list.\n");
	}
}

// Local root signature and shader association
// This is a root signature that enables a shader to have unique arguments that come from shader tables.
void PBRTViewer::CreateLocalRootSignatureSubobjects(CD3D12_STATE_OBJECT_DESC* raytracingPipeline)
{
    // Local root signature to be used in a hit group.
    auto localRootSignature = raytracingPipeline->CreateSubobject<CD3D12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    localRootSignature->SetRootSignature(m_raytracingLocalRootSignature.Get());
    // Define explicit shader association for the local root signature. 
    // In this sample, this could be ommited for convenience since it matches the default association.
    {
        auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
        rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
        rootSignatureAssociation->AddExport(c_hitGroupName);
    }

#if USE_NON_NULL_LOCAL_ROOT_SIG 
    // Empty local root signature to be used in a ray gen and a miss shader.
    {
        auto localRootSignature = raytracingPipeline->CreateSubobject<CD3D12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
        localRootSignature->SetRootSignature(m_raytracingLocalRootSignatureEmpty.Get());
        // Shader association
        auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
        rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
        rootSignatureAssociation->AddExport(c_raygenShaderName);
        rootSignatureAssociation->AddExport(c_missShaderName);
    }
#endif
}



// Build acceleration structures needed for raytracing.
void PBRTViewer::BuildAccelerationStructures()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto commandList = m_deviceResources->GetCommandList();
    auto commandQueue = m_deviceResources->GetCommandQueue();
    auto commandAllocator = m_deviceResources->GetCommandAllocator();

	g_pRenderer = CreateD3D12Renderer(commandQueue);
	auto pEnvironmentMap = g_pRenderer->CreateEnvironmentMap(
		DuosRenderer::CreateEnvironmentMapDescriptor::Panorama(g_scene.m_EnvironmentMap.m_FileName.c_str())
	);

	g_pScene = g_pRenderer->CreateScene(pEnvironmentMap);
	
	auto pfnConvertVec3 = [](SceneParser::Vector3 v)->DuosRenderer::Vec3
	{
		return DuosRenderer::Vec3(v.x, v.y, v.z);
	};

	auto pfnConvertVec2 = [](SceneParser::Vector2 v)->DuosRenderer::Vec2
	{
		return DuosRenderer::Vec2(v.x, v.y);
	};


	DuosRenderer::CreateCameraDescriptor cameraDescriptor;
	cameraDescriptor.m_Up = pfnConvertVec3(g_scene.m_Camera.m_Up);
	cameraDescriptor.m_LookAt = pfnConvertVec3(g_scene.m_Camera.m_LookAt);
	cameraDescriptor.m_FocalPoint = pfnConvertVec3(g_scene.m_Camera.m_Position);
	cameraDescriptor.m_VerticalFieldOfView = g_scene.m_Camera.m_FieldOfView;
	cameraDescriptor.m_NearClip = g_scene.m_Camera.m_NearPlane;
	cameraDescriptor.m_FarClip = g_scene.m_Camera.m_FarPlane;
	cameraDescriptor.m_Height =   static_cast<DuosRenderer::REAL>(m_height);
	cameraDescriptor.m_Width = static_cast<DuosRenderer::REAL>(m_width);
	g_pCamera = g_pRenderer->CreateCamera(cameraDescriptor);


	std::vector<std::vector<DuosRenderer::Vertex>> vertexBuffers(g_scene.m_Meshes.size());
	std::unordered_map<std::string, shared_ptr<DuosRenderer::Material>> materialMap;
	for (auto materialNameValuePair : g_scene.m_Materials)
	{
		auto &material = materialNameValuePair.second;
		DuosRenderer::CreateMaterialDescriptor desc = {};
		desc.m_DiffuseColor = pfnConvertVec3(material.m_Diffuse);
		if (material.m_DiffuseTextureFilename.size() > 0)
		{
			desc.m_TextureName = material.m_DiffuseTextureFilename.c_str();
		}
		desc.m_IndexOfRefraction = 1.5f; // TODO: Hack!
		desc.m_Roughness = material.m_URoughness;

		auto pMaterial = g_pRenderer->CreateMaterial(desc);
		materialMap[material.m_MaterialName] = pMaterial;
	}

	for (UINT i = 0; i < g_scene.m_Meshes.size(); i++)
	{
		auto &mesh = g_scene.m_Meshes[i];
		auto &vertexBuffer = vertexBuffers[i];
		vertexBuffer.reserve(mesh.m_VertexBuffer.size());

		DuosRenderer::CreateGeometryDescriptor desc;
		desc.m_pIndices = mesh.m_IndexBuffer.data();
		desc.m_NumIndices = static_cast<UINT>(mesh.m_IndexBuffer.size());
		desc.m_NumVertices = static_cast<UINT>(mesh.m_VertexBuffer.size());
		for (auto &parserVertex : mesh.m_VertexBuffer)
		{
			DuosRenderer::Vertex vertex;
			vertex.m_Normal = pfnConvertVec3(parserVertex.Normal);
			vertex.m_Position = pfnConvertVec3(parserVertex.Position);
			vertex.m_Tangent = pfnConvertVec3(parserVertex.Tangents);
			vertex.m_Tex = pfnConvertVec2(parserVertex.UV);
			vertexBuffer.push_back(vertex);
		}
		desc.m_pVertices = vertexBuffer.data();
		desc.m_pMaterial = materialMap[mesh.m_pMaterial->m_MaterialName];
		assert(desc.m_pMaterial);
		
		auto pGeometry = g_pRenderer->CreateGeometry(desc);
		g_pScene->AddGeometry(pGeometry);
	}

    // Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
    m_deviceResources->WaitForGpu();
}

void PBRTViewer::OnKeyDown(UINT8 key)
{
}

// Update frame-based values.
void PBRTViewer::OnUpdate()
{
    m_timer.Tick();
    CalculateFrameStats();
}


// Parse supplied command line args.
void PBRTViewer::ParseCommandLineArgs(WCHAR* argv[], int argc)
{
    DXSample::ParseCommandLineArgs(argv, argc);

    if (argc > 1)
    {
        if (_wcsnicmp(argv[1], L"-FL", wcslen(argv[1])) == 0 )
        {
            m_forceComputeFallback = true;
			m_raytracingAPI = RaytracingAPI::FallbackLayer;
        }
        else if (_wcsnicmp(argv[1], L"-DXR", wcslen(argv[1])) == 0)
        {
			m_raytracingAPI = RaytracingAPI::DirectXRaytracing;
        }
    }
}

// Update the application state with the new resolution.
void PBRTViewer::UpdateForSizeChange(UINT width, UINT height)
{
    DXSample::UpdateForSizeChange(width, height);
}

// Create resources that are dependent on the size of the main window.
void PBRTViewer::CreateWindowSizeDependentResources()
{
    UpdateCameraMatrices();
}

// Release resources that are dependent on the size of the main window.
void PBRTViewer::ReleaseWindowSizeDependentResources()
{
    m_raytracingOutput.Reset();
}

// Release all resources that depend on the device.
void PBRTViewer::ReleaseDeviceDependentResources()
{
    m_fallbackDevice.Reset();
    m_fallbackCommandList.Reset();
    m_fallbackStateObject.Reset();
    m_raytracingGlobalRootSignature.Reset();
    m_raytracingLocalRootSignature.Reset();
#if USE_NON_NULL_LOCAL_ROOT_SIG 
    m_raytracingLocalRootSignatureEmpty.Reset();
#endif
    
    m_dxrDevice.Reset();
    m_dxrCommandList.Reset();
    m_dxrStateObject.Reset();

    m_descriptorHeap.Reset();
    m_descriptorsAllocated = 0;
    m_raytracingOutputResourceUAVDescriptorHeapIndex = UINT_MAX;
    m_indexBuffer.resource.Reset();
    m_vertexBuffer.resource.Reset();
    m_perFrameConstants.Reset();
    m_rayGenShaderTable.Reset();
    m_missShaderTable.Reset();
    m_hitGroupShaderTable.Reset();

    m_bottomLevelAccelerationStructure.Reset();
    m_topLevelAccelerationStructure.Reset();

}

void PBRTViewer::RecreateD3D()
{
    // Give GPU a chance to finish its execution in progress.
    try
    {
        m_deviceResources->WaitForGpu();
    }
    catch (HrException&)
    {
        // Do nothing, currently attached adapter is unresponsive.
    }
    m_deviceResources->HandleDeviceLost();
}

// Render the scene.
void PBRTViewer::OnRender()
{
    if (!m_deviceResources->IsWindowVisible())
    {
        return;
    }

	m_deviceResources->Prepare();

	auto pCanvas = CreateD3D12Canvas(m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT);
	g_pRenderer->SetCanvas(pCanvas);

	DuosRenderer::RenderSettings settings(true);
	g_pRenderer->DrawScene(*g_pCamera, *g_pScene, &settings);

    m_deviceResources->Present(D3D12_RESOURCE_STATE_PRESENT);
}

void PBRTViewer::OnDestroy()
{
    // Let GPU finish before releasing D3D resources.
    m_deviceResources->WaitForGpu();
    OnDeviceLost();
}

// Release all device dependent resouces when a device is lost.
void PBRTViewer::OnDeviceLost()
{
    ReleaseWindowSizeDependentResources();
    ReleaseDeviceDependentResources();
}

// Create all device dependent resources when a device is restored.
void PBRTViewer::OnDeviceRestored()
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

// Compute the average frames per second and million rays per second.
void PBRTViewer::CalculateFrameStats()
{
    static int frameCnt = 0;
    static double elapsedTime = 0.0f;
    double totalTime = m_timer.GetTotalSeconds();
    frameCnt++;

    // Compute averages over one second period.
    if ((totalTime - elapsedTime) >= 1.0f)
    {
        float diff = static_cast<float>(totalTime - elapsedTime);
        float fps = static_cast<float>(frameCnt) / diff; // Normalize to an exact second.

        frameCnt = 0;
        elapsedTime = totalTime;

        float MRaysPerSecond = (m_width * m_height * fps) / static_cast<float>(1e6);

        wstringstream windowText;

        if (m_raytracingAPI == RaytracingAPI::FallbackLayer)
        {
            if (m_fallbackDevice->UsingRaytracingDriver())
            {
                windowText << L"(FL-DXR)";
            }
            else
            {
                windowText << L"(FL)";
            }
        }
        else
        {
            windowText << L"(DXR)";
        }
        windowText << setprecision(2) << fixed
            << L"    fps: " << fps << L"     ~Million Primary Rays/s: " << MRaysPerSecond
            << L"    GPU[" << m_deviceResources->GetAdapterID() << L"]: " << m_deviceResources->GetAdapterDescription();
        SetCustomWindowText(windowText.str().c_str());
    }
}

// Handle OnSizeChanged message event.
void PBRTViewer::OnSizeChanged(UINT width, UINT height, bool minimized)
{
    if (!m_deviceResources->WindowSizeChanged(width, height, minimized))
    {
        return;
    }

    UpdateForSizeChange(width, height);

    ReleaseWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}

// Allocate a descriptor and return its index. 
// If the passed descriptorIndexToUse is valid, it will be used instead of allocating a new one.
UINT PBRTViewer::AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse)
{
    auto descriptorHeapCpuBase = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    if (descriptorIndexToUse >= m_descriptorHeap->GetDesc().NumDescriptors)
    {
        descriptorIndexToUse = m_descriptorsAllocated++;
    }
    *cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, m_descriptorSize);
    return descriptorIndexToUse;
}

// Create SRV for a buffer.
UINT PBRTViewer::CreateBufferSRV(D3DBuffer* buffer, UINT numElements, UINT elementSize)
{
    auto device = m_deviceResources->GetD3DDevice();

    // SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.NumElements = numElements;
    if (elementSize == 0)
    {
        srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        srvDesc.Buffer.StructureByteStride = 0;
    }
    else
    {
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        srvDesc.Buffer.StructureByteStride = elementSize;
    }
    UINT descriptorIndex = AllocateDescriptor(&buffer->cpuDescriptorHandle);
    device->CreateShaderResourceView(buffer->resource.Get(), &srvDesc, buffer->cpuDescriptorHandle);
    buffer->gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, m_descriptorSize);
    return descriptorIndex;
};