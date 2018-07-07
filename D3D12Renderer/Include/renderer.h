#pragma once
#include <memory>
#include <vector>

namespace DuosRenderer
{
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
		Vec3 m_Tangent;
		Vec2 m_Tex;
	};

	class Material;

	struct CreateGeometryDescriptor
	{
		Vertex *m_pVertices;
		unsigned int m_NumVertices;

		unsigned int m_NumIndices;
		unsigned int *m_pIndices;

		std::shared_ptr<Material> m_pMaterial;
	};

	struct CreateMaterialDescriptor
	{
		const char *m_TextureName;
		const char *m_NormalMapName;
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

	// Defines which textures map to which planes for Texture Cubes
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
	class CreateEnvironmentTexturePanorama
	{
	public:
		const char *m_TextureName;
	};

	class CreateEnvironmentTextureCube
	{
	public:
		char *m_TextureNames[TEXTURES_PER_CUBE];
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
		static CreateEnvironmentMapDescriptor Panorama(const char *TextureName)
		{
			CreateEnvironmentMapDescriptor desc = {};
			desc.m_EnvironmentType = TEXTURE_PANORAMA;
			desc.m_TexturePanorama.m_TextureName = TextureName;
			return desc;
		}

		enum EnvironmentType
		{
			TEXTURE_CUBE,
			TEXTURE_PANORAMA,
			SOLID_COLOR
		} m_EnvironmentType;

		union
		{
			CreateEnvironmentTextureCube m_TextureCube;
			CreateEnvironmentTexturePanorama m_TexturePanorama;
			struct // Hack to get around Union's needing a default constructor
			{
				CreateEnvironmentColor m_SolidColor;
			};
		};
	};

	class Light
	{
	public:
		virtual ~Light() {};
	};

	class Transform
	{
	};

	class Material
	{
	public:
		virtual ~Material() {};
		virtual float GetRoughness() const = 0;
		virtual float GetReflectivity() const = 0;

		virtual void SetRoughness(float Roughness) = 0;
		virtual void SetReflectivity(float Reflectivity) = 0;
	};

	class Transformable
	{
	public:
		virtual ~Transformable() {};
		virtual void Translate(_In_ const Vec3 &translationVector) = 0;
		virtual void Rotate(float row, float yaw, float pitch) = 0;
	};

	class Camera : public Transformable
	{
	public:
		virtual ~Camera() {};
	};

	class Geometry : public Transformable
	{
	public:
		virtual ~Geometry() {};
		virtual Material &GetMaterial() const = 0;
	};

	class EnvironmentMap
	{
	public:
		virtual ~EnvironmentMap() {};
	};

	class Scene
	{
	public:
		virtual void AddGeometry(_In_ std::shared_ptr<Geometry> pGeometry) = 0;
		virtual void AddLight(_In_ std::shared_ptr<Light> pLight) = 0;
	};

	struct RenderSettings
	{
		RenderSettings(bool GammaCorrection) : m_GammaCorrection(GammaCorrection) {}
		bool operator==(const RenderSettings &RenderFlags) { return m_GammaCorrection == RenderFlags.m_GammaCorrection; }

		bool m_GammaCorrection;
	};

	const RenderSettings DefaultRenderSettings(true);

	class Renderer
	{
	public:
		virtual void SetCanvas(std::shared_ptr<Canvas> pCanvas) = 0;

		virtual void DrawScene(Camera &camera, Scene &scene, const RenderSettings &RenderFlags) = 0;

		virtual std::shared_ptr<Geometry> CreateGeometry(_In_ CreateGeometryDescriptor &desc) = 0;

		virtual std::shared_ptr<Light> CreateLight(_In_ CreateLightDescriptor &desc) = 0;

		virtual std::shared_ptr<Camera> CreateCamera(_In_ CreateCameraDescriptor &desc) = 0;

		virtual std::shared_ptr<Material> CreateMaterial(_In_ CreateMaterialDescriptor &desc) = 0;

		virtual  std::shared_ptr<EnvironmentMap> CreateEnvironmentMap(CreateEnvironmentMapDescriptor &desc) = 0;

		virtual std::shared_ptr<Scene> CreateScene(std::shared_ptr<EnvironmentMap> pEnvironmentMap) = 0;
	};
}




