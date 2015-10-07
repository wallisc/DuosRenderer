#include "RTRenderer.h"
#include "RendererException.h"

#include <atlbase.h>
#include <algorithm>
#include "glm/glm.hpp"
#include "glm/vec3.hpp"
#include "glm/gtx/transform.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"

const RTCGeometryFlags cGeometryFlag = RTC_GEOMETRY_STATIC;
const RTCSceneFlags cSceneFlags = RTC_SCENE_STATIC;

void error_handler(const RTCError code, const char* str)
{
	printf("Embree: ");
	switch (code) {
	case RTC_UNKNOWN_ERROR: printf("RTC_UNKNOWN_ERROR"); break;
	case RTC_INVALID_ARGUMENT: printf("RTC_INVALID_ARGUMENT"); break;
	case RTC_INVALID_OPERATION: printf("RTC_INVALID_OPERATION"); break;
	case RTC_OUT_OF_MEMORY: printf("RTC_OUT_OF_MEMORY"); break;
	case RTC_UNSUPPORTED_CPU: printf("RTC_UNSUPPORTED_CPU"); break;
	case RTC_CANCELLED: printf("RTC_CANCELLED"); break;
	default: printf("invalid error code"); break;
	}
	if (str) {
		printf(" (");
		while (*str) putchar(*str++);
		printf(")\n");
	}
	assert(false);
	exit(1);
}

FORCEINLINE glm::vec3 RealArrayToGlmVec3(_In_ Vec3 Vector)
{
	return glm::vec3(Vector.x, Vector.y, Vector.z);
}

FORCEINLINE RTCVertex RendererVertexToRTCVertex(Vertex &Vertex)
{
	Vec3 &Pos = Vertex.m_Position;
	return RTCVertex(Pos.x, Pos.y, Pos.z);
}

FORCEINLINE glm::vec2 RealArrayToGlmVec2(_In_ Vec2 Vector)
{
	return glm::vec2(Vector.x, Vector.y);
}

FORCEINLINE RTVertexData RendererVertexToRTVertex(Vertex &Vertex)
{
	return RTVertexData(RealArrayToGlmVec2(Vertex.m_Tex), RealArrayToGlmVec3(Vertex.m_Normal));
}

FORCEINLINE glm::vec3 RTCVertexToRendererVertex(RTCVertex Vertex)
{
	return glm::vec3(Vertex.m_x, Vertex.m_y, Vertex.m_z);
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
	m_device = rtcNewDevice();
	rtcDeviceSetErrorFunction(m_device, error_handler);

}

RTRenderer::~RTRenderer()
{
	rtcDeleteDevice(m_device);
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
	Scene *pScene = new RTScene(m_device);
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

	pRTScene->PreDraw();

	UINT Width = pRTCamera->GetWidth();
	UINT Height = pRTCamera->GetHeight();

	glm::vec3 FocalPoint = pRTCamera->GetFocalPoint();
	float PixelWidth = pRTCamera->GetLensWidth() / Width;
	float PixelHeight = pRTCamera->GetLensHeight() / Height;

	for (UINT y = 0; y < Height; y++)
	{
		for (UINT x = 0; x < Width; x++)
		{
			glm::vec3 coord(pRTCamera->GetLensWidth() * (float)x / (float)Width, pRTCamera->GetLensHeight() * (float)(Height - y) / (float)Height, 0.0f);
			coord -= glm::vec3(pRTCamera->GetLensWidth() / 2.0f, pRTCamera->GetLensHeight() / 2.0f, 0.0f);
			coord += glm::vec3(PixelWidth / 2.0f, -PixelHeight / 2.0f, 0.0f); // Make sure the ray is centered in the pixel

			glm::vec3 right = pRTCamera->GetRight();
			glm::vec3 LensPoint = pRTCamera->GetLensPosition() + coord.y * pRTCamera->GetUp() + coord.x * right;
			glm::vec3 RayDirection = glm::normalize(LensPoint - FocalPoint);

			RTCRay ray {};
			memcpy(ray.org, &LensPoint, sizeof(ray.org));
			memcpy(ray.dir, &RayDirection, sizeof(ray.dir));
			ray.tnear = 0.0f;
			ray.tfar = FLT_MAX;
			ray.geomID = RTC_INVALID_GEOMETRY_ID;
			ray.primID = RTC_INVALID_GEOMETRY_ID;
			ray.mask = -1;
			ray.time = 0;

			rtcIntersect(pRTScene->GetRTCScene(), ray);
			unsigned int meshID = ray.geomID;

			if(meshID != RTC_INVALID_GEOMETRY_ID)
			{
				RTGeometry *pGeometry = pRTScene->GetRTGeometry(meshID);
				assert(pGeometry != nullptr);

				float a = ray.u;
				float b = ray.v;
				float c = 1.0 - a - b;

				glm::vec3 TotalColor = glm::vec3(0.0f);
				glm::vec3 matColor = pGeometry->GetColor(ray.primID, a, b);
				glm::vec3 Norm = pGeometry->GetNormal(ray.primID, a, b);
				glm::vec3 intersectPos = pGeometry->GetPosition(ray.primID, a, b);
				for (RTLight *pLight : pRTScene->GetLightList())
				{
					glm::vec3 LightColor = pLight->GetLightColor(intersectPos);
					glm::vec3 LightDirection = pLight->GetLightDirection(intersectPos);

					float nDotL = glm::dot(Norm, LightDirection);
					if (nDotL > 0.0f)
					{
						glm::vec3 ShadowRayOrigin = intersectPos + LightDirection * .001f;
						RTCRay ShadowRay{};
						memcpy(ShadowRay.org, &ShadowRayOrigin, sizeof(ShadowRay.org));
						memcpy(ShadowRay.dir, &LightDirection, sizeof(ShadowRay.dir));
						ShadowRay.tnear = 0.0f;
						ShadowRay.tfar = FLT_MAX;
						ShadowRay.geomID = RTC_INVALID_GEOMETRY_ID;
						ShadowRay.primID = RTC_INVALID_GEOMETRY_ID;
						ShadowRay.mask = -1;
						ShadowRay.time = 0;

						rtcOccluded(pRTScene->GetRTCScene(), ShadowRay);
						if (ShadowRay.geomID == RTC_INVALID_GEOMETRY_ID)
						{

							TotalColor += matColor * LightColor * nDotL;
						}
					}
				}

				m_pCanvas->WritePixel(x, y, Vec3(TotalColor.r, TotalColor.g, TotalColor.b));
			}
			else
			{
				m_pCanvas->WritePixel(x, y, Vec3(1.0f, 0.0f, 1.0f));
			}
		}
	}
}

RTGeometry::RTGeometry(_In_ CreateGeometryDescriptor *pCreateGeometryDescriptor)
{
	m_pMaterial = RT_RENDERER_CAST<RTMaterial *>(pCreateGeometryDescriptor->m_pMaterial);

	m_vertexData.reserve(pCreateGeometryDescriptor->m_NumVertices);
	m_rtcVertexData.reserve(pCreateGeometryDescriptor->m_NumVertices);
	m_indexData.reserve(pCreateGeometryDescriptor->m_NumIndices);

	if (pCreateGeometryDescriptor->m_NumIndices)
	{
		for (UINT i = 0; i < pCreateGeometryDescriptor->m_NumIndices; i ++)
		{
			m_indexData.push_back(pCreateGeometryDescriptor->m_pIndices[i]);
		}

		for (UINT i = 0; i < pCreateGeometryDescriptor->m_NumVertices; i++)
		{
			auto &vertex = pCreateGeometryDescriptor->m_pVertices[i];

			m_vertexData.push_back(RendererVertexToRTVertex(vertex));
			m_rtcVertexData.push_back(RendererVertexToRTCVertex(vertex));
		}
	}
	else
	{
		for (UINT i = 0; i < pCreateGeometryDescriptor->m_NumVertices; i++)
		{
			auto &vertex = pCreateGeometryDescriptor->m_pVertices[i];

			m_vertexData.push_back(RendererVertexToRTVertex(vertex));
			m_rtcVertexData.push_back(RendererVertexToRTCVertex(vertex));

			m_indexData.push_back(i);
		}
	}
}

RTGeometry::~RTGeometry()
{
}

RTDirectionalLight::RTDirectionalLight(_In_ CreateLightDescriptor *pCreateLightDescriptor)
{
	assert(pCreateLightDescriptor->m_LightType == CreateLightDescriptor::DIRECTIONAL_LIGHT);

	m_EmissionDirection = glm::normalize(RealArrayToGlmVec3(pCreateLightDescriptor->m_pCreateDirectionalLight->m_EmissionDirection));
	m_Color = RealArrayToGlmVec3(pCreateLightDescriptor->m_Color);
}

RTScene::RTScene(RTCDevice device) :
	m_bSceneCommitted(false)
{
	m_scene = rtcDeviceNewScene(device, cSceneFlags, RTC_INTERSECT1);
}

RTScene::~RTScene()
{
	rtcDeleteScene(m_scene);
}

void RTScene::PreDraw()
{
	if (!m_bSceneCommitted)
	{
		rtcCommit(m_scene);
		m_bSceneCommitted = true;
	}
}

void RTScene::AddGeometry(_In_ Geometry *pGeometry)
{
	RTGeometry *pRTGeometry = RT_RENDERER_CAST<RTGeometry*>(pGeometry);

	UINT triangleMesh = rtcNewTriangleMesh(m_scene, cGeometryFlag, pRTGeometry->GetNumTriangles(), pRTGeometry->GetNumVertices());

	RTCVertex*pVertexBuffer = (RTCVertex*)rtcMapBuffer(m_scene, triangleMesh, RTC_VERTEX_BUFFER);
	memcpy(pVertexBuffer, pRTGeometry->GetVertexData(), pRTGeometry->GetVertexDataSize());
	rtcUnmapBuffer(m_scene, triangleMesh, RTC_VERTEX_BUFFER);

	unsigned int*pIndexBuffer = (unsigned int*)rtcMapBuffer(m_scene, triangleMesh, RTC_INDEX_BUFFER);
	memcpy(pIndexBuffer, pRTGeometry->GetIndexBufferData(), pRTGeometry->GetIndexBufferDataSize());
	rtcUnmapBuffer(m_scene, triangleMesh, RTC_INDEX_BUFFER);
	
	m_meshIDToRTGeometry[triangleMesh] = pRTGeometry;
}

void RTScene::AddLight(_In_ Light *pLight)
{
	RTLight *pRTLight = static_cast<RTLight*>(pLight);
	m_LightList.push_back(pRTLight);
}

glm::vec3 RTGeometry::GetColor(unsigned int primID, float alpha, float beta)
{
	glm::vec2 uv = GetUV(primID, alpha, beta);
	return m_pMaterial->GetColor(uv);
}

glm::vec2 RTGeometry::GetUV(unsigned int primID, float alpha, float beta)
{
	assert(m_indexData.size() > primID * 3 + 2);
	unsigned int i0 = m_indexData[primID * 3];
	unsigned int i1 = m_indexData[primID * 3 + 1];
	unsigned int i2 = m_indexData[primID * 3 + 2];

	assert(alpha + beta <= 1.0f);
	float gamma = 1.0f - alpha - beta;
	glm::vec2 uv = m_vertexData[i0].m_tex * gamma + m_vertexData[i1].m_tex * alpha + m_vertexData[i2].m_tex * beta;

	assert(uv.x <= 1.0 && uv.y <= 1.0);
	return uv;
}

glm::vec3 RTGeometry::GetPosition(unsigned int primID, float alpha, float beta)
{
	assert(m_indexData.size() > primID * 3 + 2);
	unsigned int i0 = m_indexData[primID * 3];
	unsigned int i1 = m_indexData[primID * 3 + 1];
	unsigned int i2 = m_indexData[primID * 3 + 2];

	assert(alpha + beta <= 1.0f);
	float gamma = 1.0f - alpha - beta;
	return RTCVertexToRendererVertex(m_rtcVertexData[i0]) * gamma + 
		RTCVertexToRendererVertex(m_rtcVertexData[i1]) * alpha + 
		RTCVertexToRendererVertex(m_rtcVertexData[i2]) * beta;

}

glm::vec3 RTGeometry::GetNormal(unsigned int primID, float alpha, float beta)
{
	assert(m_indexData.size() > primID * 3 + 2);
	unsigned int i0 = m_indexData[primID * 3];
	unsigned int i1 = m_indexData[primID * 3 + 1];
	unsigned int i2 = m_indexData[primID * 3 + 2];

	assert(alpha + beta <= 1.0f);
	float gamma = 1.0f - alpha - beta;
	return m_vertexData[i0].m_norm * gamma +
		m_vertexData[i1].m_norm * alpha +
		m_vertexData[i2].m_norm * beta;
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

