#include <vector>

struct Vertex
{
	float m_Position[4];
	float m_Tex[2];
};

class CreateGeometryDescriptor
{
	Vertex *m_pVertices;
	unsigned int m_NumVertices;

	unsigned int *m_pIndices;
	unsigned int m_NumIndices;
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

enum RendererExceptionType
{
	FAIL,
	OUT_OF_MEMORY
};

class RendererException
{
public:
	RendererException(RendererExceptionType type, std::string ErrorMessage) :
		m_Type(type), m_ErrorMessage(ErrorMessage) {}

	RendererExceptionType GetType() { return m_Type; }
	std::string GetErrorMessage() { return m_ErrorMessage; }
private:
	RendererExceptionType m_Type;
	std::string m_ErrorMessage;
};

class Renderer
{
public:
	virtual void CreateGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor, _Out_ Geometry **ppGeometry) = 0;
	virtual void DestroyGeometry(_In_ Geometry *pGeometry) = 0;
	virtual void CreateLight(_In_ CreateLightDescriptor *pCreateLightDescriptor, _Out_ Light **ppLight) = 0;
	virtual void DrawScene(Camera *pCamera, Scene *pScene) = 0;

protected:
	void ThrowRendererError(RendererExceptionType Type, std::string ErrorMessage) {
		throw new RendererException(Type, ErrorMessage);
	}
};

#define ORIG_CHK(expression, ExceptionType, ErrorMessage ) if(expression) { ThrowRendererError(ExceptionType, ErrorMessage); }
#define FAIL_CHK(expression, ErrorMessage ) ORIG_CHK(expression, RendererExceptionType::FAIL, ErrorMessage )
#define MEM_CHK(expression) ORIG_CHK(expression, RendererExceptionType::OUT_OF_MEMORY, "Failed new operator" )