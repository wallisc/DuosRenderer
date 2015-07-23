#pragma once
#include <vector>

#define MAX_ALLOWED_STR_LENGTH 256
typedef float REAL;

struct Vec2
{
	Vec2() : x(0), y(0) {}
	Vec2(REAL nX, REAL nY) : x(nX), y(nY) {}
	REAL x;
	REAL y;
};

struct Vec3
{
	Vec3() : x(0), y(0), z(0) {}
	Vec3(REAL nX, REAL nY, REAL nZ) : x(nX), y(nY), z(nZ) {}

	REAL x;
	REAL y;
	REAL z;
};

class ExtendableClass
{
public:
	// *ppInterface != nullptr if the Inteface is supported
	virtual void GetInterface(const char *InterfaceName, void **ppInterface) = 0;
};

class Canvas : public ExtendableClass
{
public:
	virtual void WritePixel(unsigned int x, unsigned int y, Vec3 Color) = 0;
};

struct Vertex
{
	Vec3 m_Position;
	Vec3 m_Normal;
	Vec2 m_Tex;
};

class Material;

struct CreateGeometryDescriptor
{
	Vertex *m_pVertices;
	unsigned int m_NumVertices;

	unsigned int m_NumIndices;
	unsigned int *m_pIndices;

	Material *m_pMaterial;
};

struct CreateMaterialDescriptor
{
	char *m_TextureName;
};

struct CreateDirectionalLight
{
	Vec3 m_EmissionDirection;
};

class CreateLightDescriptor
{
public:
	enum LightType
	{
		POINT_LIGHT,
		DIRECTIONAL_LIGHT
	} m_LightType;
	Vec3 m_Color;
	union
	{
		CreateDirectionalLight *m_pCreateDirectionalLight;
	};
};

class CreateCameraDescriptor
{
public:
	Vec3 m_FocalPoint;
	Vec3 m_LookAt;
	Vec3 m_Up;

	REAL m_FieldOfView;
	REAL m_NearClip;
	REAL m_FarClip;
	REAL m_Width;
	REAL m_Height;
};

class Light
{
};

class Transform
{
};

class Material
{

};

class Transformable
{
public:
	virtual void Update(_In_ Transform *pTransform) = 0;
};

class Camera : public Transformable
{
};

class Geometry : public Transformable
{
};

class Scene
{
public:
	virtual void AddGeometry(_In_ Geometry *pGeometry) = 0;
	virtual void AddLight(_In_ Light *pLight) = 0;
};

class Renderer
{
public:
	virtual void SetCanvas(Canvas *pCanvas) = 0;

	virtual void DrawScene(Camera *pCamera, Scene *pScene) = 0;

	virtual Geometry *CreateGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor) = 0;
	virtual void DestroyGeometry(_In_ Geometry *pGeometry) = 0;

	virtual Light *CreateLight(_In_ CreateLightDescriptor *pCreateLightDescriptor) = 0;
	virtual void DestroyLight(Light *pLight) = 0;

	virtual Camera *CreateCamera(_In_ CreateCameraDescriptor *pCreateCameraDescriptor) = 0;
	virtual void DestroyCamera(Camera *pCamera) = 0;

	virtual Material *CreateMaterial(_In_ CreateMaterialDescriptor *pCreateMaterialDescriptor) = 0;
	virtual void DestroyMaterial(Material* pMaterial) = 0;

	virtual Scene *CreateScene() = 0;
	virtual void DestroyScene(Scene *pScene) = 0;
};



