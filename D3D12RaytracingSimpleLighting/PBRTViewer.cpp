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

#include "D3D12Renderer.h"

#include "PBRTParser.h"
#include <algorithm>


using namespace std;
using namespace DX;

SceneParser::Scene g_scene;
shared_ptr<DuosRenderer::Renderer> g_pRenderer;
shared_ptr<DuosRenderer::Camera> g_pCamera;
shared_ptr<DuosRenderer::Scene> g_pScene;

PBRTViewer::PBRTViewer(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
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

    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

// Create resources that depend on the device.
void PBRTViewer::CreateDeviceDependentResources()
{
    BuildScene();
}

void PBRTViewer::BuildScene()
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
}

// Release resources that are dependent on the size of the main window.
void PBRTViewer::ReleaseWindowSizeDependentResources()
{
}

// Release all resources that depend on the device.
void PBRTViewer::ReleaseDeviceDependentResources()
{
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