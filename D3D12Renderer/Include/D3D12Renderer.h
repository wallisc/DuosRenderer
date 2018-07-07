#pragma once
#include "Renderer.h"
#include <d3d12.h>
#include <atlbase.h>

std::shared_ptr<DuosRenderer::Renderer> CreateD3D12Renderer(ID3D12CommandQueue *pQueue);

std::shared_ptr<DuosRenderer::Canvas> CreateD3D12Canvas(ID3D12Resource *pResource, D3D12_RESOURCE_STATES state);