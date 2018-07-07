// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <deque>

#include <d3d12.h>
#include "d3dx12.h"

#include "D3D12RaytracingFallback.h"

#include "Util.h"

#include "Renderer.h"
#include "D3D12Renderer.h"
using namespace DuosRenderer;

#include <cassert> // TODO remove when everything is implemented
#include <atlbase.h>

#include "D3D12Canvas.h"
#include "D3D12Util.h"
#include "D3D12DescriptorAllocator.h"
#include "D3D12Context.h"
#include "D3D12RendererInternal.h"
#include "D3D12Light.h"
#include "D3D12EnvironmentMap.h"
#include "D3D12Material.h"
#include "D3D12Geometry.h"
#include "RaytracingShared.h"
#include "D3D12Camera.h"
#include "D3D12Scene.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers



// TODO: reference additional headers your program requires here
