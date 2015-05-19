#include <vector>

typedef float REAL;

enum
{
	X_INDEX = 0,
	Y_INDEX,
	Z_INDEX
};

struct Vertex
{
	REAL m_Position[4];
	REAL m_Tex[2];
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
	REAL m_Position[3];
	REAL m_LookAt[3];
	REAL m_Up[3];

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
	virtual Geometry *CreateGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor) = 0;
	virtual void DestroyGeometry(_In_ Geometry *pGeometry) = 0;

	virtual Light *CreateLight(_In_ CreateLightDescriptor *pCreateLightDescriptor) = 0;
	virtual void DestroyLight(Light *pLight) = 0;

	virtual Camera *CreateCamera(_In_ CreateCameraDescriptor *pCreateCameraDescriptor) = 0;
	virtual void DestroyCamera(Camera *pCamera) = 0;

	virtual Scene *CreateScene() = 0;
	virtual void DestroyScene(Scene *pScene) = 0;
};

