#include "Renderer.h"

#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>

class RTGeometry : public Geometry
{
public:
	RTGeometry(_In_ ID3D11Device *pDevice, _In_ CreateGeometryDescriptor *pCreateGeometryDescriptor);
	~RTGeometry();

	void Update(_In_ Transform *pTransform);
};

class RTLight : public Light
{
public:
	RTLight(_In_ CreateLightDescriptor *);
};

class RTCamera : public Camera
{
public:
	RTCamera(ID3D11Device *pDevice, CreateCameraDescriptor *pCreateCameraDescriptor);
	~RTCamera();

	void Update(_In_ Transform *pTransform);
};

class RTScene : public Scene
{
public:
	void AddGeometry(_In_ Geometry *pGeometry);

	friend class RTRenderer;
protected:
	std::vector<RTGeometry *> &GetGeometryList() { return m_GeometryList; }

private:
	std::vector<RTGeometry *> m_GeometryList;

};

class RTRenderer : public Renderer
{
public:
	RTRenderer(HWND WindowHandle, unsigned int width, unsigned int height);
	Geometry *CreateGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor);
	void DestroyGeometry(_In_ Geometry *pGeometry);

	Light *CreateLight(_In_ CreateLightDescriptor *pCreateLightDescriptor);
	void DestroyLight(Light *pLight);

	Camera *CreateCamera(_In_ CreateCameraDescriptor *pCreateCameraDescriptor);
	void DestroyCamera(Camera *pCamera);

	Scene *CreateScene();
	void DestroyScene(Scene *pScene);

	void DrawScene(Camera *pCamera, Scene *pScene);

private:
	void SetDefaultState();
	void CompileShaders();
	void InitializeSwapchain(HWND WindowHandle, unsigned int width, unsigned int height);


	ID3D11Device *m_pDevice;
	ID3D11DeviceContext *m_pImmediateContext;

	IDXGISwapChain1* m_pSwapChain1;
	IDXGISwapChain* m_pSwapChain;

	ID3D11RenderTargetView* m_pSwapchainRenderTargetView;

	ID3D11VertexShader* m_pForwardVertexShader;
	ID3D11PixelShader* m_pForwardPixelShader;
	ID3D11Texture2D* m_pBackBuffer;
};

#define D3D11_RENDERER_CAST reinterpret_cast