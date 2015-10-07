#include "Renderer.h"

#include "glm/vec3.hpp"
#include "glm/vec2.hpp"

#include "embree/inc/rtcore.h"
#include "embree/inc/rtcore_scene.h"
#include "embree/inc/rtcore_ray.h"

#include <unordered_map>

class RTMaterial : public Material
{
public:
	RTMaterial(CreateMaterialDescriptor *pCreateMaterialDescriptor);

	glm::vec3 GetColor(glm::vec2 uv) 
	{
		glm::tvec2<int> coord = glm::vec2(uv.x * m_Width, uv.y * m_Height);
		unsigned char *pPixel = &m_pImage[(coord.x + coord.y * m_Width) * cSizeofComponent * cComponentCount];
		return glm::vec3(
			ConvertCharToFloat(pPixel[0]), 
			ConvertCharToFloat(pPixel[1]), 
			ConvertCharToFloat(pPixel[2]));
	}
private:
	float ConvertCharToFloat(unsigned char CharColor)
	{
		return (float)CharColor / 256.0f;
	}

	static const int cComponentCount = 3;
	static const unsigned int cSizeofComponent = sizeof(unsigned char);

	unsigned char* m_pImage;
	int m_Width;
	int m_Height;
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

class RTGeometry : public Geometry
{
public:
	RTGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor);
	~RTGeometry();

	void Rotate(float row, float yaw, float pitch) { assert(false); }
	void Translate(_In_ const Vec3 &translationVector) { assert(false); }

	RTMaterial *GetMaterial() { return m_pMaterial; }

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

class RTLight : public Light
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

class RTCamera : public Camera
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
	
	REAL m_FieldOfView;
	REAL m_NearClip;
	REAL m_FarClip;
};

class RTScene : public Scene
{
public:
	RTScene(RTCDevice);
	~RTScene();

	void AddGeometry(_In_ Geometry *pGeometry);
	void AddLight(_In_ Light *pLight);

	std::vector<RTGeometry *> &GetGeometryList() { return m_GeometryList; }
	std::vector<RTLight *> &GetLightList() { return m_LightList; }

	RTCScene GetRTCScene() { return m_scene; }
	RTGeometry *GetRTGeometry(unsigned int meshID) { return m_meshIDToRTGeometry[meshID]; }
	void PreDraw();
private:
	bool m_bSceneCommitted;
	std::vector<RTGeometry *> m_GeometryList;
	std::vector<RTLight *> m_LightList;
	std::unordered_map<unsigned int, RTGeometry *> m_meshIDToRTGeometry;

	RTCScene m_scene;
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

	Scene *CreateScene();
	void DestroyScene(Scene *pScene);

	void DrawScene(Camera *pCamera, Scene *pScene);

private:
	RTCDevice  m_device;
	Canvas *m_pCanvas;
};

#define RT_RENDERER_CAST reinterpret_cast