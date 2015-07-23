#include "Renderer.h"

#include "glm/vec3.hpp"
#include "glm/vec2.hpp"

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

class IntersectionResult;

class RTIntersectable
{
public:
	struct UVAndNormal
	{
		glm::vec2 UV;
		glm::vec3 Normal;
	};

	virtual bool Intersects(const RTRay *pRay, _Out_ IntersectionResult *pResult = nullptr) = 0;
	virtual UVAndNormal GetUVAndNormalAt(const IntersectionResult &Result) const = 0;
};

class IntersectionResult
{
public:
	IntersectionResult(const RTIntersectable *pGeometry = nullptr, float Param = -1.0f) :
		m_IntersectedGeometry(pGeometry), m_Param(Param) {}

	float GetParam() const { return m_Param; }
	const RTIntersectable* GetIntersectedGeometry() const { return m_IntersectedGeometry; }
	void SetIntersectedGeometry(const RTIntersectable *pGeometry) { m_IntersectedGeometry = pGeometry; }
	void SetParam(float Param) { m_Param = Param; }
	void SetRay(const RTRay &Ray) { m_Ray = Ray; }
	void SetMaterial(RTMaterial *pMaterial) { m_pMaterial = pMaterial; }
	RTMaterial *GetMaterial() { return m_pMaterial; }
	const RTRay &GetRay() const { return m_Ray; }
private: 
	const RTIntersectable *m_IntersectedGeometry;
	RTRay m_Ray;
	float m_Param;
	RTMaterial *m_pMaterial;
};

class RTTriangle : public RTIntersectable, public Geometry
{
public:


	RTTriangle(glm::vec3 Vertices[3], glm::vec3 Normals[3], glm::vec2 UVCoordinates[3]);
	bool Intersects(const RTRay *pRay, _Out_ IntersectionResult *pResult);

	void Update(_In_ Transform *pTransform);
	UVAndNormal GetUVAndNormalAt(const IntersectionResult &Result) const;

private:
	glm::vec3 m_V0, m_V1, m_V2;
	glm::vec3 m_N0, m_N1, m_N2;
	glm::vec2 m_Tex0, m_Tex1, m_Tex2;
	glm::vec3 m_PlaneNormal;
};

class RTGeometry : public Geometry, public RTIntersectable
{
public:
	RTGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor);
	~RTGeometry();

	void Update(_In_ Transform *pTransform);
	bool Intersects(const RTRay *pRay, _Out_ IntersectionResult *pResult = nullptr);
	UVAndNormal GetUVAndNormalAt(const IntersectionResult &Result) const { assert(false); return UVAndNormal(); }
	RTMaterial *GetMaterial() { return m_pMaterial; }

private:
	std::vector<RTTriangle> m_Mesh;
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

	void Update(_In_ Transform *pTransform);

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
	void AddGeometry(_In_ Geometry *pGeometry);
	void AddLight(_In_ Light *pLight);

	std::vector<RTGeometry *> &GetGeometryList() { return m_GeometryList; }
	std::vector<RTLight *> &GetLightList() { return m_LightList; }

private:
	std::vector<RTGeometry *> m_GeometryList;
	std::vector<RTLight *> m_LightList;

};

class RTRenderer : public Renderer
{
public:
	RTRenderer(unsigned int width, unsigned int height);
	
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
	Canvas *m_pCanvas;
};

class RTracer
{
public:
	static void Trace(RTScene *pScene, RTCamera *pCamera, Canvas *pCanvas);

private:
	static IntersectionResult Intersect(const RTRay &Ray, const std::vector<RTGeometry *> &GeometryList);
};

#define RT_RENDERER_CAST reinterpret_cast