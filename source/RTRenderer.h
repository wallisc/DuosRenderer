#include "Renderer.h"

#include "glm/vec3.hpp"
#include "glm/vec2.hpp"
#include "glm/glm.hpp"

#include "embree/inc/rtcore.h"
#include "embree/inc/rtcore_scene.h"
#include "embree/inc/rtcore_ray.h"

#include <unordered_map>
#include <algorithm>
#include <windows.h>
#include <minmax.h>

#define _USE_MATH_DEFINES
#include <math.h>
 
#define EPSILON 0.0001f
#define MEDIUM_EPSILON 0.01f
#define LARGE_EPSILON 0.1f
#define MAX_RAY_RECURSION 3
#define RAY_EMISSION_COUNT 128
#define RAYS_PER_INTERSECT_BATCH 4
#define RT_MULTITHREAD 1
#define RT_AVOID_RENDERING_REDUNDANT_FRAMES 1


class VersionedObject
{
public:
	typedef UINT VersionID;
	VersionID GetVersionID() { return m_VersionID; }
	void NotifyChanged() 
	{
		m_VersionID++;
	}
private:
	VersionID m_VersionID;
};

class Observer
{
public:
	virtual void Notify() = 0;
};

class Observable
{
public:
	void RegisterObserver(Observer *pObserver)
	{
		m_ObserverList.push_back(pObserver);
	}

	void UnregisterObserver(Observer *pObserver)
	{
		bool bObserverFound = false;
		for (auto Iterator = m_ObserverList.begin(); Iterator != m_ObserverList.end(); Iterator++)
		{
			if (*Iterator == pObserver)
			{
				m_ObserverList.erase(Iterator);
				bObserverFound = true;
				break;
			}
		}
		assert(bObserverFound);
	}

	void NotifyChanged()
	{
		for (auto pObserver : m_ObserverList)
		{
			pObserver->Notify();
		}
	}
private:
	std::vector<Observer *> m_ObserverList;
};

class RTImage
{
public:
	RTImage();
	RTImage(const char *TextureName);
	glm::vec3 Sample(glm::vec2 uv);
	bool HasValidTexture() { return m_pImage != nullptr; }
private:
	float ConvertCharToFloat(unsigned char CharColor) { return (float)CharColor / 256.0f; }

	int m_Width, m_Height;
	int m_ComponentCount = 3;
	unsigned char* m_pImage;
	static const unsigned int cSizeofComponent = sizeof(unsigned char);
};

class RTTextureCube
{
public:
	RTTextureCube();
	RTTextureCube(char **TextureNames);
	glm::vec3 Sample(glm::vec3 dir);
	bool HasValidTexture() { return m_pImages[0].HasValidTexture(); }
private:
	TextureFace GetTextureFace(glm::vec3 dir);
	RTImage m_pImages[TEXTURES_PER_CUBE];
};

class RTMaterial : public Material, public Observable
{
public:
	RTMaterial(CreateMaterialDescriptor *pCreateMaterialDescriptor);

	glm::vec3 GetColor(glm::vec2 uv);
	float GetReflectivity() const { return m_Reflectivity; }
	float GetRoughness() const { return m_Roughness; }

	void SetRoughness(float Roughness) { m_Roughness = Roughness; NotifyChanged(); }
	void SetReflectivity(float Reflectivity) { m_Reflectivity = Reflectivity; NotifyChanged(); }
private:
	glm::vec3 m_Diffuse;
	float m_Reflectivity;
	float m_Roughness;
	RTImage m_Image;
};

class RTRay
{
public:
	RTRay(glm::vec3 Origin = glm::vec3(), glm::vec3 Direction = glm::vec3()) : m_Origin(Origin), m_Direction(Direction) {}

	const glm::vec3 &GetOrigin() const { return m_Origin; }
	const glm::vec3 &GetDirection() const { return m_Direction; }
	glm::vec3 GetPoint(float t) const { return m_Origin + m_Direction * t; }
private:
	glm::vec3 m_Origin;
	glm::vec3 m_Direction;
};

class RTCVertex
{
public:
	RTCVertex(float x, float y, float z) :
		m_x(x), m_y(y), m_z(z) {}

	float m_x, m_y, m_z, m_padding;
};

class RTVertexData
{
public:
	RTVertexData(glm::vec2 tex, glm::vec3 norm) :
		m_tex(tex), m_norm(norm) {}

	glm::vec2 m_tex;
	glm::vec3 m_norm;
};

class RTTriangleData
{
public:
	RTTriangleData(RTVertexData v0, RTVertexData v1, RTVertexData v2) :
		m_v0(v0), m_v1(v1), m_v2(v2) {}

	RTVertexData m_v0, m_v1, m_v2;
	const RTVertexData &operator[](unsigned int index)
	{
		switch (index)
		{
		case 0:
			return m_v0;
		case 1:
			return m_v1;
		case 2:
			return m_v2;
		default:
			assert(false);
			return m_v0;
		}
	}
};

class RTGeometry : public Geometry, public Observable
{
public:
	RTGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor);
	~RTGeometry();

	void Rotate(float row, float yaw, float pitch) { assert(false); }
	void Translate(_In_ const Vec3 &translationVector) { assert(false); }

	RTMaterial *GetRTMaterial() const { return m_pMaterial; }
	Material *GetMaterial() const { return GetRTMaterial(); }

	unsigned int GetNumVertices() { return m_vertexData.size(); }
	unsigned int GetNumTriangles() { return m_indexData.size() / 3; }

	RTCVertex *GetVertexData() { return &m_rtcVertexData[0]; }
	size_t GetVertexDataSize() { return sizeof(RTCVertex) * m_rtcVertexData.size(); }

	unsigned int *GetIndexBufferData() { return &m_indexData[0]; }
	size_t GetIndexBufferDataSize() { return sizeof(unsigned int)* m_indexData.size(); }

	glm::vec3 GetColor(unsigned int primID, float alpha, float beta);
	glm::vec2 GetUV(unsigned int primID, float alpha, float beta);
	glm::vec3 GetPosition(unsigned int primID, float alpha, float beta);
	glm::vec3 GetNormal(unsigned int primID, float alpha, float beta);

private:
	std::vector<RTVertexData> m_vertexData;
	std::vector<unsigned int> m_indexData;
	std::vector<RTCVertex> m_rtcVertexData;
	RTMaterial *m_pMaterial;
};

class RTLight : public Light, public Observable
{
public:
	virtual glm::vec3 GetLightDirection(glm::vec3 Position) const = 0;
	virtual glm::vec3 GetLightColor(glm::vec3 Position) const = 0;
};

class RTDirectionalLight : public RTLight
{
public:
	RTDirectionalLight(_In_ CreateLightDescriptor *pCreateLightDescriptor);

	virtual glm::vec3 GetLightDirection(glm::vec3 Position) const { return -m_EmissionDirection; }
	virtual glm::vec3 GetLightColor(glm::vec3 Position) const { return m_Color; };

private:
	glm::vec3 m_EmissionDirection;
	glm::vec3 m_Color;
};

class RTCamera : public Camera, public VersionedObject
{
public:
	RTCamera(CreateCameraDescriptor *pCreateCameraDescriptor);
	~RTCamera();

	void Translate(_In_ const Vec3 &translationVector);
	void Rotate(float row, float yaw, float pitch);

	unsigned int GetWidth();
	unsigned int GetHeight();

	float GetAspectRatio();
	const glm::vec3 &GetFocalPoint();
	const glm::vec3 &GetLookAt();
	const glm::vec3 &GetUp();
	const glm::vec3 GetRight();
	const glm::vec3 GetLookDir();

	const glm::vec3 GetLensPosition();
	float GetLensWidth();
	float GetLensHeight();

private:
	unsigned int m_Width, m_Height;
	glm::vec3 m_FocalPoint;
	glm::vec3 m_LookAt;
	glm::vec3 m_Up;
	
	REAL m_VerticalFieldOfView;
	REAL m_NearClip;
	REAL m_FarClip;
};

class RTEnvironmentMap : public EnvironmentMap
{
public:
	virtual glm::vec3 GetColor(glm::vec3 ray) = 0;
};

class RTEnvironmentColor : public RTEnvironmentMap
{
public:
	RTEnvironmentColor(CreateEnvironmentColor *pCreateEnvironmentColor);
	glm::vec3 GetColor(glm::vec3 ray) { return m_Color; }
private:
	glm::vec3 m_Color;
};

class RTEnvironmentTextureCube : public RTEnvironmentMap
{
public:
	RTEnvironmentTextureCube(CreateEnvironmentTextureCube *pCreateEnvironmentTextureCube);
	glm::vec3 GetColor(glm::vec3 ray) { return m_TextureCube.Sample(ray); }

private:
	RTTextureCube m_TextureCube;
};

struct PixelRange
{
	PixelRange() : m_X(0), m_Y(0), m_Width(0), m_Height(0) {}
	PixelRange(unsigned int x, unsigned int y, unsigned int width, unsigned int height) : m_X(x), m_Y(y), m_Width(width), m_Height(height) {}

	unsigned int m_X, m_Y, m_Width, m_Height;
};

class RTScene : public Scene, public VersionedObject, public Observer
{
public:
	RTScene(RTCDevice, RTEnvironmentMap *pEnvironmentMap);
	~RTScene();

	void AddGeometry(_In_ Geometry *pGeometry);
	void AddLight(_In_ Light *pLight);

	std::vector<RTGeometry *> &GetGeometryList() { return m_GeometryList; }
	std::vector<RTLight *> &GetLightList() { return m_LightList; }

	RTCScene GetRTCScene() { return m_scene; }
	RTGeometry *GetRTGeometry(unsigned int meshID) { return meshID != RTC_INVALID_GEOMETRY_ID ? m_meshIDToRTGeometry[meshID] : nullptr; }
	RTEnvironmentMap *GetEnvironmentMap() { return m_pEnvironmentMap; }
	void Notify() { NotifyChanged(); }

	void PreDraw();
private:
	bool m_bSceneCommitted;
	
	RTEnvironmentMap *m_pEnvironmentMap;
	std::vector<RTGeometry *> m_GeometryList;
	std::vector<RTLight *> m_LightList;
	std::unordered_map<unsigned int, RTGeometry *> m_meshIDToRTGeometry;

	RTCScene m_scene;
};

class RTRenderer;
struct RayTraceThreadArgs
{
	RayTraceThreadArgs() {}
	RayTraceThreadArgs(RTRenderer *pRenderer, RTScene *pScene, RTCamera *pCamera, PixelRange range, LONG *pOngoingThreadCounter, HANDLE TracingFinishedEvent) :
		m_pRenderer(pRenderer), 
		m_pScene(pScene), 
		m_pCamera(pCamera), 
		m_PixelRange(range), 
		m_pOngoingThreadCounter(pOngoingThreadCounter),
		m_TracingFinishedEvent(TracingFinishedEvent) {}

	PixelRange m_PixelRange;
	RTRenderer *m_pRenderer;
	RTScene *m_pScene;
	RTCamera *m_pCamera;
	LONG *m_pOngoingThreadCounter;
	HANDLE m_TracingFinishedEvent;
};

class RayBatch
{
public:
	RayBatch(RTCScene Scene, _In_reads_(NumRays) const glm::vec3 *RayOrigins, _In_reads_(NumRays) const glm::vec3 *RayDirections, unsigned int NumRays);

	unsigned int GetGeometryID(unsigned int RayIndex);
	unsigned int GetPrimID(unsigned int RayIndex);
	glm::vec3 GetBaryocentricCoordinate(unsigned int RayIndex);
private:
	template<class RayType>
	unsigned int GetGeometryIDInternal(typename const RayType &RayStruct, unsigned int RayIndex)
	{
		return RayStruct.geomID[RayIndex];
	}

	template<class RayType>
	unsigned int GetPrimIDInternal(typename const RayType &RayStruct, unsigned int RayIndex)
	{
		return RayStruct.primID[RayIndex];
	}

	template<class RayType>
	void GetBaryocentricCoordinateInternal(typename const RayType &RayStruct, unsigned int RayIndex, float &u, float &v)
	{
		u = RayStruct.u[RayIndex];
		v = RayStruct.v[RayIndex];
	}

	template<class RayType>
	void InitRayStruct(typename RayType &RayStruct, _In_reads_(NumRays) const glm::vec3 *RayOrigins, _In_reads_(NumRays) const glm::vec3 *RayDirections, unsigned int NumRays)
	{
		for (UINT RayIndex = 0; RayIndex < NumRays; RayIndex++)
		{
			RayStruct.tnear[RayIndex] = 0.0f;
			RayStruct.tfar[RayIndex] = FLT_MAX;
			RayStruct.geomID[RayIndex] = RTC_INVALID_GEOMETRY_ID;
			RayStruct.primID[RayIndex] = RTC_INVALID_GEOMETRY_ID;
			RayStruct.mask[RayIndex] = -1;
			
			RayStruct.dirx[RayIndex] = RayDirections[RayIndex].x;
			RayStruct.diry[RayIndex] = RayDirections[RayIndex].y;
			RayStruct.dirz[RayIndex] = RayDirections[RayIndex].z;
			
			RayStruct.orgx[RayIndex] = RayOrigins[RayIndex].x;
			RayStruct.orgy[RayIndex] = RayOrigins[RayIndex].y;
			RayStruct.orgz[RayIndex] = RayOrigins[RayIndex].z;
			ValidMask[RayIndex] = -1;
		}

		for (UINT RayIndex = NumRays; RayIndex < 16; RayIndex++)
		{
			ValidMask[RayIndex] = 0;
		}
	}

	unsigned int m_NumRays;
	__declspec(align(16)) int ValidMask[16];

	union
	{
		RTCRay Ray;
		__declspec(align(16)) RTCRay4 Ray4;
		__declspec(align(64)) RTCRay16 Ray16;
	};
};

class RTRenderer : public Renderer
{
public:
	RTRenderer(unsigned int width, unsigned int height);
	~RTRenderer();
	
	void SetCanvas(Canvas *pCanvas) { m_pCanvas = pCanvas; }
	Geometry *CreateGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor);
	void DestroyGeometry(_In_ Geometry *pGeometry);

	Light *CreateLight(_In_ CreateLightDescriptor *pCreateLightDescriptor);
	void DestroyLight(Light *pLight);

	Camera *CreateCamera(_In_ CreateCameraDescriptor *pCreateCameraDescriptor);
	void DestroyCamera(Camera *pCamera);

	Material *CreateMaterial(_In_ CreateMaterialDescriptor *pCreateMaterialDescriptor);
	void DestroyMaterial(Material* pMaterial);

	EnvironmentMap *CreateEnvironmentMap(CreateEnvironmentMapDescriptor *pCreateEnvironmnetMapDescriptor);
	void DestroyEnviromentMap(EnvironmentMap *pEnvironmentMap);

	Scene *CreateScene(EnvironmentMap *pEnvironmentMap);
	void DestroyScene(Scene *pScene);

	void DrawScene(Camera *pCamera, Scene *pScene);
	Geometry *GetGeometryAtPixel(Camera *pCamera, Scene *pScene, Vec2 PixelCoord);

	void RenderPixelRange(PixelRange *pRange, RTCamera *pCamera, RTScene *pScene);
private:
	struct ShadePixelRecursionInfo
	{
		ShadePixelRecursionInfo(UINT NumRecursions = 1, float TotalContribution = 1.0f) :
			m_NumRecursions(NumRecursions), m_TotalContribution(TotalContribution)
		{}

		UINT m_NumRecursions;
		float m_TotalContribution;
	};

	void Trace(_In_ RTScene *pScene, _In_reads_(NumRays) const glm::vec3 *pRayOrigins, _In_reads_(NumRays) const glm::vec3 *pRayDirs, _Out_ glm::vec3 *pColors, UINT NumRays, ShadePixelRecursionInfo &RecursionInfo);
	glm::vec3 ShadePixel(RTScene *pScene, unsigned int primID, RTGeometry *pGeometry, glm::vec3 baryocentricCoord, glm::vec3 ViewVector, ShadePixelRecursionInfo &RecursionInfo);

	PTP_POOL m_ThreadPool;
	PTP_CLEANUP_GROUP m_ThreadPoolCleanupGroup;

	RTCDevice  m_device;
	Canvas *m_pCanvas;

	RTCamera *m_pLastCamera;
	RTScene *m_pLastScene;
	LONG m_RunningThreadCounter;
	std::vector<RayTraceThreadArgs> m_ThreadArgs;
	HANDLE m_TracingFinishedEvent;

	const bool m_bEnableMultiRayEmission = false;
	VersionedObject::VersionID m_LastCameraVersionID;
	VersionedObject::VersionID m_LastSceneID;
};

class BRDFShader
{
public:
	virtual float BRDF(_In_ const glm::vec3 &ViewVector, _In_ const glm::vec3 &Normal, _In_ const glm::vec3 &IncomingRadianceVector, _In_ float roughness, _In_ float baseReflectivity, _Out_ float &FresnelFactor) = 0;
};

typedef float(*FresnelFunction)(const glm::vec3 &Normal, const glm::vec3 &HalfwayVector, float BaseReflectivity);
typedef float(*DistributionFunction)(const glm::vec3 &Normal, const glm::vec3 &IncomingRadianceDirection, float Roughness);
typedef float(*GeometryAttenuationFunction)(const glm::vec3 &ViewDirection, const glm::vec3 &Normal, const glm::vec3 &HalfwayVector, float Roughness);

static float saturate(float n)
{
	return min(1.0f, max(n, 0.0f));
}

static float Fresnel_ShlicksApproximation(const glm::vec3 &Normal, const glm::vec3 &HalfwayVector, float BaseReflectivity)
{
	return (BaseReflectivity + (1.0 - BaseReflectivity) * pow(1.0f - saturate(glm::dot(HalfwayVector, Normal)), 5));
}

static float GeometryAttenuationPartial_Schlicks(const glm::vec3 &Normal, const glm::vec3 &IncomingRadiationDirection, float k)
{
	float nDotV = saturate(glm::dot(Normal, IncomingRadiationDirection));
	return nDotV / (nDotV * (1 - k) + k);
}

static float GeometricAttenuation_Schlick(const glm::vec3 &ViewDirection, const glm::vec3 &Normal, const glm::vec3 &IncomingRadianceDirection, float Roughness)
{
	float k = Roughness * sqrt(2.0 / M_PI);
	return GeometryAttenuationPartial_Schlicks(Normal, ViewDirection, k) * GeometryAttenuationPartial_Schlicks(Normal, IncomingRadianceDirection, k);
}


static float Distribution_GGX(const glm::vec3 &Normal, const glm::vec3 &HalfwayVector, float Roughness)
{
	float nDotH = saturate(glm::dot(Normal, HalfwayVector));
	float nDotHSquared = nDotH * nDotH;
	float roughnessSquared = Roughness * Roughness;
	return roughnessSquared / (M_PI * pow(nDotHSquared * (roughnessSquared - 1) + 1, 2.0));
}

class CookTorrance : public BRDFShader
{
public:
	float BRDF(_In_ const glm::vec3 &ViewVector, _In_ const glm::vec3 &Normal, _In_ const glm::vec3 &IncomingRadianceVector, _In_ float roughness, _In_ float baseReflectivity, _Out_ float &FresnelFactor);

private:
	const FresnelFunction F = Fresnel_ShlicksApproximation;
	const DistributionFunction D = Distribution_GGX;
	const GeometryAttenuationFunction G = GeometricAttenuation_Schlick;
};


const UINT g_EmbreeCompatibleIntersectCountInDescendingOrder[] = { 8, 4, 2, 1 };
inline bool IsIntersectCountEmbreeCompatible(UINT num)
{
	for (UINT count : g_EmbreeCompatibleIntersectCountInDescendingOrder)
	{
		if (count == num) return true;
	}
	return false;
}

inline UINT TruncateToCompatibleEmbreeCompatible(UINT num)
{
	assert(num != 0);
	for (UINT count : g_EmbreeCompatibleIntersectCountInDescendingOrder)
	{
		if (num >= count) return count;
	}
}




#define RT_RENDERER_CAST reinterpret_cast