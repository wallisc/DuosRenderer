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

#pragma once

#include "DXSample.h"
#include "StepTimer.h"
#include "RaytracingHlslCompat.h"

class PBRTViewer : public DXSample
{
    enum class RaytracingAPI {
        FallbackLayer,
        DirectXRaytracing,
    };

public:
    PBRTViewer(UINT width, UINT height, std::wstring name);

    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    // Messages
    virtual void OnInit();
    virtual void OnKeyDown(UINT8 key);
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnSizeChanged(UINT width, UINT height, bool minimized);
    virtual void OnDestroy();
    virtual IDXGISwapChain* GetSwapchain() { return m_deviceResources->GetSwapChain(); }

private:
    static const UINT FrameCount = 3;
    bool m_isDxrSupported;

    // Application state
    RaytracingAPI m_raytracingAPI;
    bool m_forceComputeFallback;
    StepTimer m_timer;

    void EnableDXRExperimentalFeatures(IDXGIAdapter1* adapter);
    void ParseCommandLineArgs(WCHAR* argv[], int argc);
    void RecreateD3D();
    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    void ReleaseDeviceDependentResources();
    void ReleaseWindowSizeDependentResources();
    void BuildScene();
    void UpdateForSizeChange(UINT clientWidth, UINT clientHeight);
    void CalculateFrameStats();
};
