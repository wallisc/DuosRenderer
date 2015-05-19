#include "Renderer.h"

#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>

class D3D11Geometry : public Geometry
{
public:
	D3D11Geometry(_In_ ID3D11Device *pDevice, _In_ CreateGeometryDescriptor *pCreateGeometryDescriptor);
	~D3D11Geometry();

	void Update(_In_ Transform *pTransform);

	ID3D11Buffer *GetVertexBuffer() const { return m_pVertexBuffer; }
	ID3D11Buffer *GetIndexBuffer() const { return m_pIndexBuffer; }
	unsigned int GetVertexCount() const { return m_VertexCount; }
	unsigned int GetIndexCount() const { return m_IndexCount; }
private:
	bool m_UsesIndexBuffer;
	unsigned int m_VertexCount;
	unsigned int m_IndexCount;
	ID3D11Buffer* m_pVertexBuffer;
	ID3D11Buffer* m_pIndexBuffer;
};

class D3D11Light : public Light
{
public:
	D3D11Light(_In_ CreateLightDescriptor *);
private:
	ID3D11Buffer* m_pLightConstantBuffer;
};

class D3D11Camera : public Camera
{
public:
	D3D11Camera(ID3D11Device *pDevice, CreateCameraDescriptor *pCreateCameraDescriptor);
	~D3D11Camera();

	void Update(_In_ Transform *pTransform);

private:
	REAL m_Width;
	REAL m_Height;
	DirectX::XMMATRIX m_ViewMatrix;
};

class D3D11Scene : public Scene
{
public:
	void AddGeometry(_In_ Geometry *pGeometry);

	friend class D3D11Renderer;
protected:
	std::vector<D3D11Geometry *> &GetGeometryList() { return m_GeometryList; }

private:
	std::vector<D3D11Geometry *> m_GeometryList;
	
};

class D3D11Renderer : public Renderer
{
public:
	D3D11Renderer(HWND WindowHandle, unsigned int width, unsigned int height);
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

	ID3D11VertexShader* m_pForwardVertexShader;
	ID3D11PixelShader* m_pForwardPixelShader;
	ID3D11InputLayout* m_pForwardInputLayout;
};

#define D3D11_RENDERER_CAST reinterpret_cast