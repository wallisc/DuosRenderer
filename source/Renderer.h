#pragma once
#include <vector>

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

struct Vertex
{
	Vec3 m_Position;
	Vec3 m_Normal;
	Vec2 m_Tex;
};

struct CreateGeometryDescriptor
{
	Vertex *m_pVertices;
	unsigned int m_NumVertices;

	unsigned int m_NumIndices;
	unsigned int *m_pIndices;
};

class CreateDirectionalLight
{
	float m_Direction[4];
};

class CreateLightDescriptor
{
public:
	enum LightType
	{
		POINT_LIGHT,
		DIRECTIONAL_LIGHT
	} m_LightType;
	union
	{
		CreateDirectionalLight m_CreateDirectionalLight;
	};
};

class CreateCameraDescriptor
{
public:
	Vec3 m_Position;
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
};

class Renderer
{
public:
	virtual void DrawScene(Camera *pCamera, Scene *pScene) = 0;

	virtual Geometry *CreateGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor) = 0;
	virtual void DestroyGeometry(_In_ Geometry *pGeometry) = 0;

	virtual Light *CreateLight(_In_ CreateLightDescriptor *pCreateLightDescriptor) = 0;
	virtual void DestroyLight(Light *pLight) = 0;

	virtual Camera *CreateCamera(_In_ CreateCameraDescriptor *pCreateCameraDescriptor) = 0;
	virtual void DestroyCamera(Camera *pCamera) = 0;

	virtual Scene *CreateScene() = 0;
	virtual void DestroyScene(Scene *pScene) = 0;
};

