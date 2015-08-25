#include "RTRenderer.h"
#include "RendererException.h"

#include <atlbase.h>
#include <algorithm>
#include "glm/glm.hpp"
#include "glm/vec3.hpp"
#include "glm/gtx/transform.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"


glm::vec3 RealArrayToGlmVec3(_In_ Vec3 Vector)
{
	return glm::vec3(Vector.x, Vector.y, Vector.z);
}

glm::vec2 RealArrayToGlmVec2(_In_ Vec2 Vector)
{
	return glm::vec2(Vector.x, Vector.y);
}

RTMaterial::RTMaterial(CreateMaterialDescriptor *pCreateMaterialDescriptor)
{
	int ComponentCount;
	m_pImage = stbi_load(pCreateMaterialDescriptor->m_TextureName, &m_Width, &m_Height, &ComponentCount, STBI_rgb);
	FAIL_CHK(ComponentCount != cComponentCount, "Stb_Image returned an unexpected component count");
}

Material *RTRenderer::CreateMaterial(_In_ CreateMaterialDescriptor *pCreateMaterialDescriptor)
{
	Material *pMaterial = new RTMaterial(pCreateMaterialDescriptor);
	MEM_CHK(pMaterial);
	return pMaterial;
}

void RTRenderer::DestroyMaterial(Material* pMaterial)
{
	delete pMaterial;
}

RTRenderer::RTRenderer(unsigned int width, unsigned int height)
{
}

Geometry* RTRenderer::CreateGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor)
{
	RTGeometry* pGeometry = new RTGeometry(pCreateGeometryDescriptor);
	MEM_CHK(pGeometry);
	return pGeometry;
}

void RTRenderer::DestroyGeometry(_In_ Geometry *pGeometry)
{
	delete pGeometry;
}

Light *RTRenderer::CreateLight(_In_ CreateLightDescriptor *pCreateLightDescriptor)
{
	Light *pLight = nullptr;
	switch (pCreateLightDescriptor->m_LightType)
	{
	case CreateLightDescriptor::DIRECTIONAL_LIGHT:
		pLight = new RTDirectionalLight(pCreateLightDescriptor);
		break;
	default:
		assert(false);
	}

	MEM_CHK(pLight);
	return pLight;
}

void RTRenderer::DestroyLight(Light *pLight)
{
	delete pLight;
}

Camera *RTRenderer::CreateCamera(_In_ CreateCameraDescriptor *pCreateCameraDescriptor)
{
	Camera *pCamera = new RTCamera(pCreateCameraDescriptor);
	MEM_CHK(pCamera);
	return pCamera;
}

void RTRenderer::DestroyCamera(Camera *pCamera)
{
	delete pCamera;
}

Scene *RTRenderer::CreateScene()
{
	Scene *pScene = new RTScene();
	MEM_CHK(pScene);
	return pScene;
}

void RTRenderer::DestroyScene(Scene* pScene)
{
	delete pScene;
}

void RTRenderer::DrawScene(Camera *pCamera, Scene *pScene)
{
	RTScene *pRTScene = RT_RENDERER_CAST<RTScene*>(pScene);
	RTCamera *pRTCamera = RT_RENDERER_CAST<RTCamera*>(pCamera);

	UINT m_Width = 800;
	UINT m_Height = 600;

	RTracer::Trace(pRTScene, pRTCamera, m_pCanvas);
}

RTGeometry::RTGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor)
{
	m_pMaterial = RT_RENDERER_CAST<RTMaterial *>(pCreateGeometryDescriptor->m_pMaterial);
	if (pCreateGeometryDescriptor->m_NumIndices)
	{
		for (UINT i = 0; i < pCreateGeometryDescriptor->m_NumIndices; i += 3)
		{
			UINT Index0 = pCreateGeometryDescriptor->m_pIndices[i];
			UINT Index1 = pCreateGeometryDescriptor->m_pIndices[i + 1];
			UINT Index2 = pCreateGeometryDescriptor->m_pIndices[i + 2];

			glm::vec3 Triangle[3];
			glm::vec2 UVCoordinates[3];
			glm::vec3 Normals[3];

			Triangle[0] = RealArrayToGlmVec3(pCreateGeometryDescriptor->m_pVertices[Index0].m_Position);
			Triangle[1] = RealArrayToGlmVec3(pCreateGeometryDescriptor->m_pVertices[Index1].m_Position);
			Triangle[2] = RealArrayToGlmVec3(pCreateGeometryDescriptor->m_pVertices[Index2].m_Position);

			UVCoordinates[0] = RealArrayToGlmVec2(pCreateGeometryDescriptor->m_pVertices[Index0].m_Tex);
			UVCoordinates[1] = RealArrayToGlmVec2(pCreateGeometryDescriptor->m_pVertices[Index1].m_Tex);
			UVCoordinates[2] = RealArrayToGlmVec2(pCreateGeometryDescriptor->m_pVertices[Index2].m_Tex);

			Normals[0] = RealArrayToGlmVec3(pCreateGeometryDescriptor->m_pVertices[Index0].m_Normal);
			Normals[1] = RealArrayToGlmVec3(pCreateGeometryDescriptor->m_pVertices[Index1].m_Normal);
			Normals[2] = RealArrayToGlmVec3(pCreateGeometryDescriptor->m_pVertices[Index2].m_Normal);
			m_Mesh.push_back(RTTriangle(Triangle, Normals, UVCoordinates));
		}
	}
	else
	{
		for (UINT i = 0; i < pCreateGeometryDescriptor->m_NumVertices; i += 3)
		{
			glm::vec3 Triangle[3];
			Triangle[0] = RealArrayToGlmVec3(pCreateGeometryDescriptor->m_pVertices[i].m_Position);
			Triangle[1] = RealArrayToGlmVec3(pCreateGeometryDescriptor->m_pVertices[i + 1].m_Position);
			Triangle[2] = RealArrayToGlmVec3(pCreateGeometryDescriptor->m_pVertices[i + 2].m_Position);

			glm::vec2 UVCoordinates[3];
			UVCoordinates[0] = RealArrayToGlmVec2(pCreateGeometryDescriptor->m_pVertices[i].m_Tex);
			UVCoordinates[1] = RealArrayToGlmVec2(pCreateGeometryDescriptor->m_pVertices[i + 1].m_Tex);
			UVCoordinates[2] = RealArrayToGlmVec2(pCreateGeometryDescriptor->m_pVertices[i + 2].m_Tex);

			glm::vec3 Normals[3];
			Normals[0] = RealArrayToGlmVec3(pCreateGeometryDescriptor->m_pVertices[i].m_Normal);
			Normals[1] = RealArrayToGlmVec3(pCreateGeometryDescriptor->m_pVertices[i + 1].m_Normal);
			Normals[2] = RealArrayToGlmVec3(pCreateGeometryDescriptor->m_pVertices[i + 2].m_Normal);
			m_Mesh.push_back(RTTriangle(Triangle, Normals, UVCoordinates));
		}
	}
}

RTGeometry::~RTGeometry()
{
}

bool RTGeometry::Intersects(const RTRay *pRay, IntersectionResult *pResult)
{
	bool bFindExactIntersectingPrimitive = pResult != nullptr;

	IntersectionResult ClosestResult(nullptr, FLT_MAX);
	for (RTTriangle &Triangle : m_Mesh)
	{
		IntersectionResult Result;
		if (Triangle.Intersects(pRay, &Result))
		{
			if (bFindExactIntersectingPrimitive)
			{
				if (Result.GetParam() < ClosestResult.GetParam())
				{
					ClosestResult = Result;
				}
			}
			else
			{
				// We can exit early since we don't need to find the closest intersection
				return true;
			}
		}
	}

	if (bFindExactIntersectingPrimitive)
	{
		*pResult = ClosestResult;
		pResult->SetMaterial(GetMaterial());
	}
	return ClosestResult.GetParam() >= 0.0f && ClosestResult.GetParam() != FLT_MAX;
}

RTDirectionalLight::RTDirectionalLight(_In_ CreateLightDescriptor *pCreateLightDescriptor)
{
	assert(pCreateLightDescriptor->m_LightType == CreateLightDescriptor::DIRECTIONAL_LIGHT);

	m_EmissionDirection = glm::normalize(RealArrayToGlmVec3(pCreateLightDescriptor->m_pCreateDirectionalLight->m_EmissionDirection));
	m_Color = RealArrayToGlmVec3(pCreateLightDescriptor->m_Color);
}

void RTScene::AddGeometry(_In_ Geometry *pGeometry)
{
	RTGeometry *pRTGeometry = RT_RENDERER_CAST<RTGeometry*>(pGeometry);
	m_GeometryList.push_back(pRTGeometry);
}

void RTScene::AddLight(_In_ Light *pLight)
{
	RTLight *pRTLight = static_cast<RTLight*>(pLight);
	m_LightList.push_back(pRTLight);
}

RTCamera::RTCamera(CreateCameraDescriptor *pCreateCameraDescriptor) :
	m_Width(pCreateCameraDescriptor->m_Width), m_Height(pCreateCameraDescriptor->m_Height),
	m_NearClip(pCreateCameraDescriptor->m_NearClip), m_FarClip(pCreateCameraDescriptor->m_FarClip),
	m_FieldOfView(pCreateCameraDescriptor->m_FieldOfView)
{
	m_FocalPoint = RealArrayToGlmVec3(pCreateCameraDescriptor->m_FocalPoint);
 	m_LookAt = RealArrayToGlmVec3(pCreateCameraDescriptor->m_LookAt);
	m_Up = RealArrayToGlmVec3(pCreateCameraDescriptor->m_Up);
}

RTCamera::~RTCamera()
{

}

const glm::vec3 &RTCamera::GetFocalPoint()
{
	return m_FocalPoint;
}

float RTCamera::GetAspectRatio()
{
	return (float)m_Width / (float)m_Height;
}

const glm::vec3 RTCamera::GetLensPosition()
{
	float FocalLength = 1.0f / tan(m_FieldOfView / 2.0f);
	return m_FocalPoint + GetLookDir() * FocalLength;
}

const glm::vec3& RTCamera::GetLookAt()
{
	return m_LookAt;
}

const glm::vec3& RTCamera::GetUp()
{
	return m_Up;
}

const glm::vec3 RTCamera::GetLookDir()
{

	return glm::normalize(m_LookAt - m_FocalPoint);
}

const glm::vec3 RTCamera::GetRight()
{

	return glm::cross(m_Up, GetLookDir());
}


UINT RTCamera::GetWidth() { return m_Width; }
UINT RTCamera::GetHeight() { return m_Height; }

float RTCamera::GetLensWidth() { return 2.0f * GetAspectRatio(); }
float RTCamera::GetLensHeight() { return 2.0f; }

void RTCamera::Translate(_In_ const Vec3 &translationVector)
{
	glm::vec3 lookDir = glm::normalize(m_LookAt - m_FocalPoint);
	glm::vec3 right = glm::cross(glm::vec3(lookDir), m_Up);

	glm::vec3 delta = translationVector.x * right + translationVector.y * m_Up + translationVector.z * lookDir;

	m_FocalPoint += delta;
	m_LookAt += delta;
}

void RTCamera::Rotate(float row, float yaw, float pitch)
{
	glm::vec4 lookDir = glm::vec4(glm::normalize(m_LookAt - m_FocalPoint), 0.0f);

	if (pitch)
	{
		glm::vec3 right = glm::cross(glm::vec3(lookDir), m_Up);
		glm::mat4 pitchRotation = glm::rotate(pitch, right);
		lookDir = pitchRotation * lookDir;
		m_Up = glm::vec3(pitchRotation * glm::vec4(m_Up, 0.0f));
	}

	if (yaw)
	{
		glm::mat4 yawRotation = glm::rotate(yaw, m_Up);
		lookDir = yawRotation * lookDir;
	}

	m_LookAt = m_FocalPoint + glm::vec3(lookDir) * glm::length(m_LookAt - m_FocalPoint);
}


void RTracer::Trace(RTScene *pScene, RTCamera *pCamera, Canvas *pCanvas)
{
	UINT Width = pCamera->GetWidth();
	UINT Height = pCamera->GetHeight();

	glm::vec3 FocalPoint = pCamera->GetFocalPoint();
	float PixelWidth = pCamera->GetLensWidth() / Width;
	float PixelHeight = pCamera->GetLensHeight() / Height;

	for (UINT y = 0; y < Height; y++)
	{
		for (UINT x = 0; x < Width; x++)
		{
 			glm::vec3 coord(pCamera->GetLensWidth() * (float)x / (float)Width, pCamera->GetLensHeight() * (float)(Height - y) / (float)Height, 0.0f);
			coord -= glm::vec3(pCamera->GetLensWidth() / 2.0f, pCamera->GetLensHeight() / 2.0f, 0.0f);
			coord += glm::vec3(PixelWidth / 2.0f, -PixelHeight / 2.0f, 0.0f); // Make sure the ray is centered in the pixel

			glm::vec3 right = pCamera->GetRight();
			glm::vec3 LensPoint = pCamera->GetLensPosition() + coord.y * pCamera->GetUp() + coord.x * right;
			glm::vec3 RayDirection = glm::normalize(LensPoint - FocalPoint);

			RTRay Ray(LensPoint, LensPoint - FocalPoint);
			bool Intersected = false;
			IntersectionResult DirectTraceResult = Intersect(Ray, pScene->GetGeometryList());
			if (DirectTraceResult.GetParam() >= 0.0f && DirectTraceResult.GetParam() != FLT_MAX)
			{
				RTIntersectable::UVAndNormal UVAndNormalResult = DirectTraceResult.GetIntersectedGeometry()->GetUVAndNormalAt(DirectTraceResult);
				glm::vec2 uv = UVAndNormalResult.UV;
				glm::vec3 n = UVAndNormalResult.Normal;
				glm::vec3 Point = DirectTraceResult.GetRay().GetPoint(DirectTraceResult.GetParam());
				glm::vec3 TextureColor = DirectTraceResult.GetMaterial()->GetColor(uv);
				glm::vec3 TotalColor(0.0f);

				for (RTLight *pLight : pScene->GetLightList())
				{
					glm::vec3 LightColor = pLight->GetLightColor(Point);
					glm::vec3 LightDirection = pLight->GetLightDirection(Point);

					RTRay ShadowRay(Point + .001f * LightDirection, LightDirection);
					IntersectionResult ShadowResult = Intersect(ShadowRay, pScene->GetGeometryList());
					
					// Only shade if the shadow feeler didn't hit anything
					if (ShadowResult.GetParam() == FLT_MAX)
					{
						float nDotL = glm::dot(LightDirection, n);
						if (nDotL > 0.0f)
						{
							TotalColor += nDotL * LightColor * TextureColor;
						}
					}
				}

				pCanvas->WritePixel(x, y, Vec3(TotalColor.r, TotalColor.g, TotalColor.b));
			}
			else
			{
				pCanvas->WritePixel(x, y, Vec3(0.0f, 0.0f, 1.0f));
			}

		}
	}
}

IntersectionResult RTracer::Intersect(const RTRay &Ray, const std::vector<RTGeometry *> &GeometryList)
{
	IntersectionResult ClosestResult(nullptr, FLT_MAX);
	for (RTGeometry *pGeometry : GeometryList)
	{
		IntersectionResult Result;
		if (pGeometry->Intersects(&Ray, &Result))
		{
			if (Result.GetParam() < ClosestResult.GetParam())
			{
				ClosestResult = Result;
			}
		}
	}

	return ClosestResult;
}


RTTriangle::RTTriangle(glm::vec3 Vertices[3], glm::vec3 Normals[3], glm::vec2 UVCoordinates[3])
{
	m_V0 = Vertices[0];
	m_V1 = Vertices[1];
	m_V2 = Vertices[2];

	m_Tex0 = UVCoordinates[0];
	m_Tex1 = UVCoordinates[1];
	m_Tex2 = UVCoordinates[2];
	
	m_N0 = Normals[0];
	m_N1 = Normals[1];
	m_N2 = Normals[2];
	m_PlaneNormal = glm::normalize(glm::cross(m_V1 - m_V0, m_V2 - m_V0));

}

bool RTTriangle::Intersects(const RTRay *pRay, IntersectionResult *pResult)
{
	float numer = glm::dot(-m_PlaneNormal, pRay->GetOrigin() - m_V0);
	float denom = glm::dot(m_PlaneNormal, pRay->GetDirection());

	if (denom == 0.0f || numer == 0.0f)
		return false;

	float t = numer / denom;

	if (t < 0.0f) return false;

	// intersection point
	glm::vec3 Intersection = pRay->GetPoint(t);

	glm::vec3 A = m_V0, B = m_V1, C = m_V2;
	glm::vec3 AB = B - A;
	glm::vec3 AC = C - A;

	glm::vec3 AP = Intersection - A;
	glm::vec3 ABxAP = glm::cross(AB, AP);
	float v_num = glm::dot(m_PlaneNormal, ABxAP);
	if (v_num < 0.0f) return false;

	// edge 2
	glm::vec3 BP = Intersection - B;
	glm::vec3 BC = C - B;
	glm::vec3 BCxBP = glm::cross(BC, BP);
	if (glm::dot(m_PlaneNormal, BCxBP) < 0.0f)
		return false; // P is on the left side

	// edge 3, needed to compute u
	glm::vec3 CP = Intersection - C;
	// we have computed AC already so we can avoid computing CA by
	// inverting the vectors in the cross product:
	// Cross(CA, CP) = cross(CP, AC);
	glm::vec3 CAxCP = glm::cross(CP, AC);
	float u_num = glm::dot(m_PlaneNormal, CAxCP);
	if (u_num < 0.0f)
		return false; // P is on the left side;

	// TODO: This is nasty design, should just make some sort of constructor for this
	pResult->SetIntersectedGeometry(this);
	pResult->SetParam(t);
	pResult->SetRay(*pRay);
	return t > 0.0f;
}

RTTriangle::UVAndNormal RTTriangle::GetUVAndNormalAt(const IntersectionResult &IntersectResult) const
{
	UVAndNormal Result;

	glm::vec3 q = IntersectResult.GetRay().GetPoint(IntersectResult.GetParam());
	float area = glm::dot(glm::cross(m_V1 - m_V0, m_V2 - m_V0), m_PlaneNormal);
	float beta = glm::dot(glm::cross(m_V0 - m_V2, q - m_V2), m_PlaneNormal) / area;
	float gamma = glm::dot(glm::cross(m_V1 - m_V0, q - m_V0), m_PlaneNormal) / area;
	float alpha = 1.0f - beta - gamma;

	Result.UV = alpha * m_Tex0 + beta * m_Tex1 + gamma * m_Tex2;
	Result.Normal = alpha * m_N0 + beta * m_N1 + gamma * m_N2;
	return Result;
}

