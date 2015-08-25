#include "Renderer.h"

#include <d3d11_1.h>
#include <directxmath.h>
#include "D3D11Canvas.h"

struct CBCamera
{
	DirectX::XMVECTOR m_CamPos;
	DirectX::XMVECTOR m_Dimensions;
	DirectX::XMVECTOR m_ClipDistance;
};

struct CBViewProjectionTransforms
{
	DirectX::XMMATRIX m_View;
	DirectX::XMMATRIX m_Projection;
};

struct CBDirectionalLight
{
	DirectX::XMVECTOR m_Direction;
	DirectX::XMVECTOR m_Color;
};

class D3D11Material : public Material
{
public:
	D3D11Material(_In_ ID3D11Device *pDevice, CreateMaterialDescriptor *pCreateMaterialDescriptor);
	ID3D11ShaderResourceView *GetShaderResourceView() const { return pTextureResourceView; }
private:
	ID3D11ShaderResourceView *pTextureResourceView;
};

class D3D11Transformable : public Transformable
{
public:
	D3D11Transformable(_In_ ID3D11Device *pDevice, _In_ ID3D11DeviceContext *pImmediateContext);
	virtual void Translate(_In_ const Vec3 &translationVector);
	virtual void Rotate(float row, float yaw, float pitch) { assert(false); }


protected:
	ID3D11Buffer *GetWorldTransformBuffer();
	const DirectX::XMMATRIX &GetWorldTransform() { return m_WorldMatrix; }

private:
	ID3D11DeviceContext *m_pImmediateContext;
	ID3D11Buffer *m_pWorldTransform;
	DirectX::XMMATRIX m_WorldMatrix;
};

class D3D11Geometry : public D3D11Transformable, public Geometry
{
public:
	D3D11Geometry(_In_ ID3D11Device *pDevice, _In_ ID3D11DeviceContext *pImmediateContext, _In_ CreateGeometryDescriptor *pCreateGeometryDescriptor);
	~D3D11Geometry();

	ID3D11Buffer *GetVertexBuffer() const { return m_pVertexBuffer; }
	ID3D11Buffer *GetIndexBuffer() const { return m_pIndexBuffer; }
	unsigned int GetVertexCount() const { return m_VertexCount; }
	unsigned int GetIndexCount() const { return m_IndexCount; }
	D3D11Material *GetMaterial() const { return m_pMaterial; }

	DirectX::XMVECTOR GetMaxDimensions() const { return m_MaxDimensions; }
	DirectX::XMVECTOR GetMinDimensions() const { return m_MinDimensions; }

	virtual void Translate(_In_ const Vec3 &translationVector) { D3D11Transformable::Translate(translationVector); }
	virtual void Rotate(float row, float yaw, float pitch) { D3D11Transformable::Rotate(row, yaw, pitch); }


private:
	bool m_UsesIndexBuffer;
	unsigned int m_VertexCount;
	unsigned int m_IndexCount;
	ID3D11Buffer* m_pVertexBuffer;
	ID3D11Buffer* m_pIndexBuffer;
	D3D11Material *m_pMaterial;

	DirectX::XMVECTOR m_MaxDimensions;
	DirectX::XMVECTOR m_MinDimensions;
};

class D3D11Light : public Light
{
public:
	D3D11Light(CreateLightDescriptor::LightType Type) :
		m_Type(Type) {}

	CreateLightDescriptor::LightType GetLightType() const { return m_Type; }
private:
	CreateLightDescriptor::LightType m_Type;
};

class D3D11Scene; // Forward Declaration
class D3D11Camera; // Forward Declaration

class D3D11DirectionalLight : public D3D11Light
{
public:
	D3D11DirectionalLight(_In_ ID3D11Device *pDevice, _In_ CreateLightDescriptor *pCreateDirectionalLight);

	ID3D11Buffer *GetDirectionalLightConstantBuffer() const { return m_pLightConstantBuffer; }
	
	// The position from where we render the shadow buffer depends on the scene
	ID3D11Buffer *GetViewProjBuffer(ID3D11DeviceContext *pContext, D3D11Scene *pScene, D3D11Camera *pCamera);
private:
	CBDirectionalLight m_CpuLightData;
	CBViewProjectionTransforms m_ViewProjCpuData;

	ID3D11Buffer* m_pLightConstantBuffer;
	ID3D11Buffer *m_pViewProjBuffer;
};

class D3D11Camera : public Camera
{
public:
	D3D11Camera(ID3D11Device *pDevice, ID3D11DeviceContext *pContext, CreateCameraDescriptor *pCreateCameraDescriptor);
	~D3D11Camera();

	virtual void Translate(_In_ const Vec3 &translationVector);
	virtual void Rotate(float row, float yaw, float pitch);

	ID3D11Buffer *GetCameraConstantBuffer();
	ID3D11Buffer *GetViewProjBuffer();
	float GetAspectRatio() { return m_Width / m_Height; }
private:
	CBCamera m_CameraCpuData;
	CBViewProjectionTransforms m_ViewProjCpuData;

	ID3D11DeviceContext *m_pContext;

	ID3D11Buffer *m_pCameraBuffer;
	ID3D11Buffer *m_pViewProjBuffer;

	REAL m_Width;
	REAL m_Height;

	bool m_ViewMatrixNeedsUpdate;

	DirectX::XMMATRIX m_ViewMatrix;
	DirectX::XMVECTOR m_Position;
	DirectX::XMVECTOR m_LookAt;
	DirectX::XMVECTOR m_Up;
};

class D3D11Scene : public Scene
{
public:
	D3D11Scene();

	void AddGeometry(_In_ Geometry *pGeometry);
	void AddLight(_In_ Light *pLight);

	DirectX::XMVECTOR GetMaxDimensions() const { return m_MaxDimensions; }
	DirectX::XMVECTOR GetMinDimensions() const { return m_MinDimensions; }

	friend class D3D11Renderer;
protected:
	std::vector<D3D11Geometry *> &GetGeometryList() { return m_GeometryList; }
	std::vector<D3D11DirectionalLight *> &GetDirectionalLightList() { return m_DirectionalLightList; }

private:
	std::vector<D3D11Geometry *> m_GeometryList;
	std::vector<D3D11DirectionalLight *> m_DirectionalLightList;

	DirectX::XMVECTOR m_MaxDimensions;
	DirectX::XMVECTOR m_MinDimensions;
};

class D3D11Renderer : public Renderer
{
public:
	D3D11Renderer(HWND WindowHandle, unsigned int width, unsigned int height);
	
	void SetCanvas(Canvas* pCanvas);

	Geometry *CreateGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor);
	void DestroyGeometry(_In_ Geometry *pGeometry);
	
	Light *CreateLight(_In_ CreateLightDescriptor *pCreateLightDescriptor);
	void DestroyLight(Light *pLight);

	Camera *CreateCamera(_In_ CreateCameraDescriptor *pCreateCameraDescriptor);
	void DestroyCamera(Camera *pCamera);

	Material *CreateMaterial(_In_ CreateMaterialDescriptor *pCreateMaterialDescriptor);
	void DestroyMaterial(Material* pMaterial);

	Scene *CreateScene();
	void DestroyScene(Scene *pScene);

	void DrawScene(Camera *pCamera, Scene *pScene);

private:
	void SetDefaultState();
	void CompileShaders();
	void InitializeSwapchain(HWND WindowHandle, unsigned int width, unsigned int height);


	ID3D11Device *m_pDevice;
	ID3D11DeviceContext *m_pImmediateContext;

	D3D11Canvas *m_pCanvas;

	ID3D11RenderTargetView* m_pSwapchainRenderTargetView;
	ID3D11DepthStencilView* m_pDepthBuffer;

	ID3D11DepthStencilView* m_pShadowDepthBuffer;
	ID3D11ShaderResourceView *m_pShadowResourceView;

	ID3D11VertexShader* m_pForwardVertexShader;
	ID3D11PixelShader* m_pForwardPixelShader;
	ID3D11InputLayout* m_pForwardInputLayout;

	ID3D11SamplerState *m_pSamplerState;
};

#define D3D11_RENDERER_CAST reinterpret_cast