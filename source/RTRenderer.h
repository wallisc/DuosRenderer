#include "Renderer.h"

#include <d3d11_1.h>
#include "glm/vec3.hpp"

class RTRay
{
public:
	RTRay(glm::vec3 Origin, glm::vec3 Direction) : m_Origin(Origin), m_Direction(Direction) {}

	const glm::vec3 &GetOrigin() const { return m_Origin; }
	const glm::vec3 &GetDirection() const { return m_Direction; }
	glm::vec3 GetPoint(float t) { return m_Origin + m_Direction * t;  }
private:
	glm::vec3 m_Origin;
	glm::vec3 m_Direction;
};

class RTIntersectable
{
	virtual float Intersects(RTRay *pRay) = 0;
};

class RTTriangle : public RTIntersectable
{
public:
	RTTriangle(glm::vec3 Vertices[3]);
	float Intersects(RTRay *pRay);
private:
	glm::vec3 m_V0, m_V1, m_V2;
	glm::vec3 m_Normal;
};

class RTGeometry : public Geometry, public RTIntersectable
{
public:
	RTGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor);
	~RTGeometry();

	void Update(_In_ Transform *pTransform);
	float Intersects(RTRay *pRay);
private:
	std::vector<RTTriangle> m_Mesh;
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

	UINT GetWidth();
	UINT GetHeight();
	glm::vec3 GetFocalPoint();
	float GetAspectRatio();
	const glm::vec3 &GetPosition();
	const glm::vec3 &GetLookAt();

private:
	UINT m_Width, m_Height;
	glm::vec3 m_Position;
	glm::vec3 m_LookAt;
	glm::vec3 m_Up;
	
	REAL m_FieldOfView;
	REAL m_NearClip;
	REAL m_FarClip;
};

class RTScene : public Scene
{
public:
	void AddGeometry(_In_ Geometry *pGeometry);

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
	void InitializeSwapchain(HWND WindowHandle, unsigned int width, unsigned int height);

	ID3D11Device *m_pDevice;
	ID3D11DeviceContext *m_pImmediateContext;

	IDXGISwapChain1* m_pSwapChain1;
	IDXGISwapChain* m_pSwapChain;

	ID3D11Texture2D* m_pBackBuffer;
	ID3D11Texture2D *m_pMappableTexture;
};

class RTCanvas
{ 
public:
	virtual void WritePixel(UINT x, UINT y, Vec3 Color) = 0;
};

class RTD3DMappedCanvas : public RTCanvas
{
public:
	RTD3DMappedCanvas(void *pMappedSurface, UINT RowPitch, DXGI_FORMAT Format);
	void WritePixel(UINT x, UINT y, Vec3 Color);

private:
	UINT m_RowPitch;
	void *m_pMappedSurface;
};

class RTracer
{
public:
	static void Trace(RTScene *pScene, RTCamera *pCamera, RTCanvas *pCanvas);
};

#define RT_RENDERER_CAST reinterpret_cast