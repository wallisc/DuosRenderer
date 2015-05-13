#include "Renderer.h"
#include <d3d11_1.h>

class D3D11Geometry : public Geometry
{
public:
	D3D11Geometry(_In_ ID3D11Device *pDevice, _In_ CreateGeometryDescriptor *pCreateGeometryDescriptor);
	void Update(_In_ Transform *pTransform);
private:
	bool m_UsesIndexBuffer;
	ID3D11Buffer* m_pVertexBuffer;
	ID3D11Buffer* m_pIndexBuffer;
};

class D3D11Light : public Light
{
public:
	D3D11Light(_In_ CreateLightDescriptor *) {}
};

class D3D11Renderer : public Renderer
{
public:
	D3D11Renderer();
	void CreateGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor, _Out_ Geometry **ppGeometry);
	void DestroyGeometry(_In_ Geometry *pGeometry);
	void CreateLight(_In_ CreateLightDescriptor *pCreateLightDescriptor, _Out_ Light **ppLight);
	void DrawScene(Camera *pCamera, Scene *pScene);

private:
	ID3D11Device *m_pDevice;
	ID3D11DeviceContext *m_pImmediateContext;
};