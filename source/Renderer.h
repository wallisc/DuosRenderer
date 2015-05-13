#include <vector>

struct Vertex
{
	float m_Position[4];
	float m_Tex[2];
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

class Light
{
};

class Camera
{
};

class Transform
{
};

class Geometry
{
public:
	virtual void Update(_In_ Transform *pTransform) = 0;
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
	virtual Scene *CreateScene() = 0;
	virtual void DestroyScene(Scene *pScene) = 0;
};

