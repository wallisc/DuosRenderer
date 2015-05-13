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
	virtual void Update(_In_ Transform *pTransform) = 0;
};

class Scene
{
	virtual void AddGeometry(_In_ Geometry *pGeometry) = 0;
};

class Renderer
{
public:
	virtual void CreateGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor, _Out_ Geometry **ppGeometry) = 0;
	virtual void DestroyGeometry(_In_ Geometry *pGeometry) = 0;
	virtual void CreateLight(_In_ CreateLightDescriptor *pCreateLightDescriptor, _Out_ Light **ppLight) = 0;
	virtual void DrawScene(Camera *pCamera, Scene *pScene) = 0;
};

