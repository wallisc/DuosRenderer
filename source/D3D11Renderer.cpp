#include "D3D11Renderer.h"

D3D11Renderer::D3D11Renderer()
{
	UINT CeateDeviceFlags = 0;
#ifdef DEBUG
	CeateDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	D3D_FEATURE_LEVEL FeatureLevels = D3D_FEATURE_LEVEL_11_1;
	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, CeateDeviceFlags, &FeatureLevels, 1,
		D3D11_SDK_VERSION, &m_pDevice, nullptr, &m_pImmediateContext);

	FAIL_CHK(FAILED(hr), "Failed D3D11CreateDevice");
}

void D3D11Renderer::CreateGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor, _Out_ Geometry **ppGeometry)
{
	*ppGeometry = new D3D11Geometry(pCreateGeometryDescriptor);
	MEM_CHK(ppGeometry);

}

void D3D11Renderer::DestroyGeometry(_In_ Geometry *pGeometry)
{
	delete pGeometry;
}

void D3D11Renderer::CreateLight(_In_ CreateLightDescriptor *pCreateLightDescriptor, _Out_ Light **ppLight)
{
	*ppLight = new D3D11Light(pCreateLightDescriptor);
	MEM_CHK(ppLight);
}

void D3D11Renderer::DrawScene(Camera *pCamera, Scene *pScene)
{
}

D3D11Geometry::D3D11Geometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor) {}
void D3D11Geometry::Update(_In_ Transform *pTransform) {}