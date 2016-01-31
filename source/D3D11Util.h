#include "RendererException.h"

#include <d3d11_1.h>
#include <directxmath.h>


ID3D11Buffer *CreateConstantBuffer(ID3D11Device *pDevice, void *InitialData, size_t DataSize, D3D11_USAGE Usage = D3D11_USAGE_DEFAULT)
{
    D3D11_BUFFER_DESC BufferDesc;
    ZeroMemory(&BufferDesc, sizeof(BufferDesc));
    BufferDesc.Usage = Usage;
    BufferDesc.ByteWidth = DataSize;
    BufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    BufferDesc.CPUAccessFlags = Usage == D3D11_USAGE_DYNAMIC ? D3D11_CPU_ACCESS_WRITE : 0;

    D3D11_SUBRESOURCE_DATA InitalDataDesc;
    ZeroMemory(&InitalDataDesc, sizeof(InitalDataDesc));
    InitalDataDesc.pSysMem = InitialData;

    ID3D11Buffer *pBuffer;
    HRESULT hr = pDevice->CreateBuffer(&BufferDesc, &InitalDataDesc, &pBuffer);
    FAIL_CHK(FAILED(hr), "Failed creating a constant buffer");

    return pBuffer;
}