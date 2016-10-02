#pragma once
#include "Renderer.h"
#include "RendererException.h"
#include <Windows.h>
#include <d3d11_1.h>
#include <string>
#include <atlbase.h>

static const char *D3D11CanvasKey = "D3D11Canvas";
class D3D11Canvas : public Canvas
{
public:
    D3D11Canvas(ID3D11Device *pDevice, ID3D11DeviceContext *pContext, UINT Width, UINT Height) :
        m_pContext(pContext), m_pMappedData(nullptr), PixelsWritten(false)
    {
        InitializeCriticalSection(&m_MapLock);

        D3D11_TEXTURE2D_DESC CanvasDesc;
        ZeroMemory(&CanvasDesc, sizeof(CanvasDesc));
        CanvasDesc.Width = Width;
        CanvasDesc.Height = Height;
        CanvasDesc.MipLevels = 1;
        CanvasDesc.ArraySize = 1;
        CanvasDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        CanvasDesc.SampleDesc.Count = 1;
        CanvasDesc.SampleDesc.Quality = 0;
        CanvasDesc.Usage = D3D11_USAGE_DEFAULT;
        CanvasDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        CanvasDesc.CPUAccessFlags = 0;
        CanvasDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;
        HRESULT hr = pDevice->CreateTexture2D(&CanvasDesc, nullptr, &m_pResource);
        FAIL_CHK(FAILED(hr), "Failed creating a canvas resource");

        D3D11_TEXTURE2D_DESC StagingDesc;
        ZeroMemory(&StagingDesc, sizeof(StagingDesc));
        StagingDesc.Width = Width;
        StagingDesc.Height = Height;
        StagingDesc.MipLevels = 1;
        StagingDesc.ArraySize = 1;
        StagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        StagingDesc.SampleDesc.Count = 1;
        StagingDesc.SampleDesc.Quality = 0;
        StagingDesc.Usage = D3D11_USAGE_DYNAMIC;
        StagingDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        StagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        StagingDesc.MiscFlags = 0;
        hr = pDevice->CreateTexture2D(&StagingDesc, nullptr, &m_pStagingResource);
        FAIL_CHK(FAILED(hr), "Failed creating a canvas resource");

        CComPtr<IDXGIResource1> pDXGIResource(NULL);
        hr = m_pResource->QueryInterface(__uuidof(IDXGIResource1), (void**)&pDXGIResource);
        FAIL_CHK(FAILED(hr), "Failed querying for IDXGIResource interface");
        
        hr = pDXGIResource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, L"", &m_ResourceHandle);
        FAIL_CHK(FAILED(hr), "Failed getting a shared handle");

        Map();
    }

    ~D3D11Canvas()
    {
        DeleteCriticalSection(&m_MapLock);
    }

    HANDLE GetCanvasResourceHandle() { return m_ResourceHandle; }

    // The resource returned from GetCanvasResource() is only valid for the current frame. 
    // If WritePixel() is called afterward, the app must use GetCanvasResource to see if the canvas
    // resource may have changed
    ID3D11Resource *GetCanvasResource() 
    { 
        if (m_pMappedData)
        {
            Unmap();
        }
        return m_pResource; 
    }

    void WritePixel(unsigned int x, unsigned int y, Vec3 Color)
    {
        assert(m_pMappedData != nullptr);

        byte *pPixel = ((byte *)m_pMappedData) + m_RowPitch * y + x * sizeof(byte)* 4;
        pPixel[0] = Color.x * 255.0f;
        pPixel[1] = Color.y * 255.0f;
        pPixel[2] = Color.z * 255.0f;
        pPixel[3] = 0;
        PixelsWritten = true;
    }

    void GetInterface(const char *InterfaceName, void **ppInterface)
    {
        if (std::string(InterfaceName) == std::string(D3D11CanvasKey))
        {
            *ppInterface = this;
        }
    }

private:
    void Map()
    {
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        HRESULT hr = m_pContext->Map(m_pStagingResource, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
        FAIL_CHK(FAILED(hr), "Failed to map canvas resource");
        m_pMappedData = MappedResource.pData;
        m_RowPitch = MappedResource.RowPitch;
    }

    void Unmap()
    {
        m_pContext->Unmap(m_pStagingResource, 0);
        m_pMappedData = nullptr;
        if (PixelsWritten)
        {
            m_pContext->CopyResource(m_pResource, m_pStagingResource);
        }

        PixelsWritten = false;
        Map();
    }

    bool PixelsWritten;
    ID3D11DeviceContext *m_pContext;
    CComPtr<ID3D11Texture2D> m_pResource;
    CComPtr<ID3D11Texture2D> m_pStagingResource;
    unsigned int m_RowPitch;
    HANDLE m_ResourceHandle;
    mutable void* m_pMappedData;
    CRITICAL_SECTION m_MapLock;
};