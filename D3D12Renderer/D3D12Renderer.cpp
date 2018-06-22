#include "stdafx.h"
#include "D3D12Renderer.h"
#include "D3D12RendererInternal.h"

Renderer *CreateD3D12Renderer(ID3D12Device *pDevice)
{
    return new D3D12Renderer(pDevice);
}

D3D12Renderer::D3D12Renderer(ID3D12Device *pDevice) :
    m_pDevice(pDevice)
{

}

void D3D12Renderer::SetCanvas(Canvas *pCanvas)
{
}