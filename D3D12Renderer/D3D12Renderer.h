#pragma once
#include "Renderer.h"
#include <d3d12.h>

Renderer *CreateD3D12Renderer(ID3D12Device *pDevice);