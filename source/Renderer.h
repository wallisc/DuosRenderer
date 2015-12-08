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
	const char *m_TextureName;
	Vec3 m_DiffuseColor;
	REAL m_Reflectivity;
	REAL m_Roughness;
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

	REAL m_VerticalFieldOfView;
	REAL m_NearClip;
	REAL m_FarClip;
	REAL m_Width;
	REAL m_Height;
};

// Defines which textures map to which plan for Texture Cubes
enum TextureFace
{
	POS_Z = 0,
	NEG_Z,
	POS_Y,
	NEG_Y,
	POS_X,
	NEG_X,
	TEXTURES_PER_CUBE
};

class CreateEnvironmentTextureCube
{
public:
	char *m_TextureNames[TEXTURES_PER_CUBE];
	char *m_IrradianceTextureNames[TEXTURES_PER_CUBE];
};

class CreateEnvironmentColor
{
public:
	Vec3 m_Color;
};


class CreateEnvironmentMapDescriptor
{
public:
	CreateEnvironmentMapDescriptor() {}

	enum EnvironmentType
	{
		TEXTURE_CUBE,
		SOLID_COLOR
	} m_EnvironmentType;

	union
	{
		CreateEnvironmentTextureCube m_TextureCube;
		struct // Hack to get around Union's needing a default constructor
		{
			CreateEnvironmentColor m_SolidColor;
		};
	};
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
	virtual void Translate(_In_ const Vec3 &translationVector) = 0;
	virtual void Rotate(float row, float yaw, float pitch) = 0;
};

class Camera : public Transformable
{
};

class Geometry : public Transformable
{
};

class EnvironmentMap
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
	virtual Geometry *GetGeometryAtPixel(Camera *pCamera, Scene *pScene, Vec2 PixelCoord) = 0;

	virtual Geometry *CreateGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor) = 0;
	virtual void DestroyGeometry(_In_ Geometry *pGeometry) = 0;

	virtual Light *CreateLight(_In_ CreateLightDescriptor *pCreateLightDescriptor) = 0;
	virtual void DestroyLight(Light *pLight) = 0;

	virtual Camera *CreateCamera(_In_ CreateCameraDescriptor *pCreateCameraDescriptor) = 0;
	virtual void DestroyCamera(Camera *pCamera) = 0;

	virtual Material *CreateMaterial(_In_ CreateMaterialDescriptor *pCreateMaterialDescriptor) = 0;
	virtual void DestroyMaterial(Material* pMaterial) = 0;

	virtual EnvironmentMap *CreateEnvironmentMap(CreateEnvironmentMapDescriptor *pCreateEnvironmnetMapDescriptor) = 0;
	virtual void DestroyEnviromentMap(EnvironmentMap *pEnvironmentMap) = 0;

	virtual Scene *CreateScene(EnvironmentMap *pEnvironmentMap) = 0;
	virtual void DestroyScene(Scene *pScene) = 0;
};



