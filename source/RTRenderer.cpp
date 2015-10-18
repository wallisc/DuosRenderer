#include "RTRenderer.h"
#include "RendererException.h"

#include <atlbase.h>
#include <algorithm>
#include <queue>
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

void FailWithLastError()     
{
	DWORD retSize;
	LPTSTR pTemp = NULL;

	retSize = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_ARGUMENT_ARRAY,
		NULL,
		GetLastError(),
		LANG_NEUTRAL,
		(LPTSTR)&pTemp,
		0,
		NULL);

	char ErrorBuffer[1024];
	sprintf(ErrorBuffer, "%0.*s (0x%x)", sizeof(ErrorBuffer), pTemp, GetLastError());

	FAIL_CHK(true, std::string(ErrorBuffer));
}

FORCEINLINE glm::vec3 RealArrayToGlmVec3(_In_ Vec3 Vector)
{
	return glm::vec3(Vector.x, Vector.y, Vector.z);
}

FORCEINLINE Vec3 GlmVec3ToRealArray(_In_ glm::vec3 Vector)
{
	return Vec3(Vector.x, Vector.y, Vector.z);
}

RTImage::RTImage() : m_pImage(nullptr) {}

RTImage::RTImage(const char *TextureName) : m_pImage(nullptr)
{
	const bool ValidTextureString = (TextureName != nullptr && strlen(TextureName) > 0);
	if (ValidTextureString)
	{
		int ComponentCount;
		m_pImage = stbi_load(TextureName, &m_Width, &m_Height, &ComponentCount, STBI_rgb);
		FAIL_CHK(ComponentCount != 3 && ComponentCount != 4, "Stb_Image returned an unexpected component count");
	}
}


glm::vec3 RTImage::Sample(glm::vec2 uv)
{
	assert(m_pImage);
	glm::tvec2<int> coord = glm::vec2(uv.x * m_Width, uv.y * m_Height);
	unsigned char *pPixel = &m_pImage[(coord.x + coord.y * m_Width) * cSizeofComponent * m_ComponentCount];
	return glm::vec3(
		ConvertCharToFloat(pPixel[0]),
		ConvertCharToFloat(pPixel[1]),
		ConvertCharToFloat(pPixel[2]));
}

RTTextureCube::RTTextureCube() {}
RTTextureCube::RTTextureCube(char **TextureNames)
{
	for (UINT i = 0; i < TEXTURES_PER_CUBE; i++)
	{
		m_pImages[i] = RTImage(TextureNames[i]);
	}
}

TextureFace RTTextureCube::GetTextureFace(glm::vec3 dir)
{
	const float absZ = fabsf(dir.z);
	const float absY = fabsf(dir.y);
	const float absX = fabsf(dir.x);
	if (absZ > absX && absZ > absY)
	{
		return dir.z > 0.0 ? POS_Z : NEG_Z;
	}
	else if (absY > absX)
	{
		return dir.y > 0.0f ? POS_Y : NEG_Y;
	}
	else
	{
		assert(absX >= absZ && absX >= absY);
		return dir.x > 0.0f ? POS_X : NEG_X;
	}
}

glm::vec3 RTTextureCube::Sample(glm::vec3 dir)
{
	const TextureFace Face = GetTextureFace(dir);
	glm::vec2 uv;
	switch (Face)
	{
	case POS_X:
		uv = glm::vec2((dir.z / dir.x + 1.0f) * 0.5,
			1.0f - (dir.y / dir.x + 1.0f) * 0.5);
		break;
	case NEG_X:
		uv = glm::vec2((dir.z / dir.x + 1.0f) * 0.5,
			(dir.y / dir.x + 1.0f) * 0.5);
		break;
	case POS_Y:
		uv = glm::vec2((dir.z / dir.y + 1.0f) * 0.5,
			(dir.x / dir.y + 1.0f) * 0.5);
		break;
	case NEG_Y:
		uv = glm::vec2(1.0f - (dir.z / dir.y + 1.0f) * 0.5,
			(dir.x / dir.y + 1.0f) * 0.5);
		break;
	case POS_Z:
		uv = glm::vec2(1.0f - (dir.x / dir.z + 1.0f) * 0.5,
			1.0f - (dir.y / dir.z + 1.0f) * 0.5);
		break;
	case NEG_Z:
		uv = glm::vec2(1.0f - (dir.x / dir.z + 1.0f) * 0.5,
			(dir.y / dir.z + 1.0f) * 0.5);
		break;
	}
	assert(uv.x >= -EPSILON && uv.x <= 1.0f + EPSILON);
	assert(uv.y >= -EPSILON && uv.y <= 1.0f + EPSILON);
	return m_pImages[Face].Sample(uv);
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
	m_Image = RTImage(pCreateMaterialDescriptor->m_TextureName);
	m_Diffuse = RealArrayToGlmVec3(pCreateMaterialDescriptor->m_DiffuseColor);
	m_Reflectivity = pCreateMaterialDescriptor->m_Reflectivity;
}

glm::vec3 RTMaterial::GetColor(glm::vec2 uv)
{
	if (m_Image.HasValidTexture())
	{
		return m_Image.Sample(uv);
	}
	else
	{
		return m_Diffuse;
	}
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

EnvironmentMap *RTRenderer::CreateEnvironmentMap(CreateEnvironmentMapDescriptor *pCreateEnvironnentMapDescriptor)
{
	EnvironmentMap *pEnvironmentMap;
	switch (pCreateEnvironnentMapDescriptor->m_EnvironmentType)
	{
	case CreateEnvironmentMapDescriptor::SOLID_COLOR:
		pEnvironmentMap = new RTEnvironmentColor(&pCreateEnvironnentMapDescriptor->m_SolidColor);
		break;
	case CreateEnvironmentMapDescriptor::TEXTURE_CUBE:
		pEnvironmentMap = new RTEnvironmentTextureCube(&pCreateEnvironnentMapDescriptor->m_TextureCube);
		break;
	}
	MEM_CHK(pEnvironmentMap);
	return pEnvironmentMap;
}

void RTRenderer::DestroyEnviromentMap(EnvironmentMap *pEnvironmentMap)
{
	delete pEnvironmentMap;
}

RTRenderer::RTRenderer(unsigned int width, unsigned int height)
{
	m_device = rtcNewDevice();
	rtcDeviceSetErrorFunction(m_device, error_handler);

	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	const UINT numWorkerThreads = sysinfo.dwNumberOfProcessors;
	m_ThreadPool = CreateThreadpool(nullptr);
	if (!m_ThreadPool) FailWithLastError();

	m_ThreadPoolCleanupGroup = CreateThreadpoolCleanupGroup();
	if (!m_ThreadPoolCleanupGroup) FailWithLastError();
	
	TP_CALLBACK_ENVIRON PoolEnvironment;
	InitializeThreadpoolEnvironment(&PoolEnvironment);
	SetThreadpoolCallbackCleanupGroup(&PoolEnvironment, m_ThreadPoolCleanupGroup, nullptr);

	SetThreadpoolThreadMaximum(m_ThreadPool, numWorkerThreads * 3);
	SetThreadpoolThreadMinimum(m_ThreadPool, 1);

	m_TracingFinishedEvent = CreateEvent(nullptr, true, false, nullptr);
	m_pLastScene = nullptr;
	m_pLastCamera = nullptr;
}

RTRenderer::~RTRenderer()
{
	rtcDeleteDevice(m_device);
	CloseThreadpool(m_ThreadPool);
	CloseThreadpoolCleanupGroup(m_ThreadPoolCleanupGroup);
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

Scene *RTRenderer::CreateScene(EnvironmentMap *pEnvironmentMap)
{
	RTEnvironmentMap *pRTEnvironmentMap = (RTEnvironmentMap *)pEnvironmentMap;
	Scene *pScene = new RTScene(m_device, pRTEnvironmentMap);
	MEM_CHK(pScene);
	return pScene;
}

void RTRenderer::DestroyScene(Scene* pScene)
{
	delete pScene;
}

void RTRenderer::RenderPixelRange(PixelRange *pRange, RTCamera *pCamera, RTScene *pScene)
{
	assert(pRange->m_Width > 0 && pRange->m_X + pRange->m_Width <= pCamera->GetWidth());
	assert(pRange->m_Height > 0 && pRange->m_Y + pRange->m_Height <= pCamera->GetHeight());
	for (UINT x = pRange->m_X; x < pRange->m_X + pRange->m_Width; x++)
	{
		for (UINT y = pRange->m_Y; y < pRange->m_Y + pRange->m_Height; y++)
		{
			RenderPixel(x, y, pCamera, pScene);
		}
	}
}

void RTRenderer::RenderPixel(unsigned int x, unsigned int y, RTCamera *pCamera, RTScene *pScene)
{
	const unsigned int Width = pCamera->GetWidth();
	const unsigned int Height = pCamera->GetHeight();
	const glm::vec3 FocalPoint = pCamera->GetFocalPoint();
	const float PixelWidth = pCamera->GetLensWidth() / Width;
	const float PixelHeight = pCamera->GetLensHeight() / Height;

	glm::vec3 coord(pCamera->GetLensWidth() * (float)x / (float)Width, pCamera->GetLensHeight() * (float)(Height - y) / (float)Height, 0.0f);
	coord -= glm::vec3(pCamera->GetLensWidth() / 2.0f, pCamera->GetLensHeight() / 2.0f, 0.0f);
	coord += glm::vec3(PixelWidth / 2.0f, -PixelHeight / 2.0f, 0.0f); // Make sure the ray is centered in the pixel

	glm::vec3 right = pCamera->GetRight();
	glm::vec3 LensPoint = pCamera->GetLensPosition() + coord.y * pCamera->GetUp() + coord.x * right;
	glm::vec3 RayDirection = glm::normalize(LensPoint - FocalPoint);

	RTCRay ray{};
	memcpy(ray.org, &LensPoint, sizeof(ray.org));
	memcpy(ray.dir, &RayDirection, sizeof(ray.dir));
	ray.tnear = 0.0f;
	ray.tfar = FLT_MAX;
	ray.geomID = RTC_INVALID_GEOMETRY_ID;
	ray.primID = RTC_INVALID_GEOMETRY_ID;
	ray.mask = -1;
	ray.time = 0;

	rtcIntersect(pScene->GetRTCScene(), ray);
	unsigned int meshID = ray.geomID;

	if (meshID != RTC_INVALID_GEOMETRY_ID)
	{
		RTGeometry *pGeometry = pScene->GetRTGeometry(meshID);
		assert(pGeometry != nullptr);

		float a = ray.u;
		float b = ray.v;
		float c = 1.0 - a - b;

		glm::vec3 TotalColor = glm::vec3(0.0f);
		glm::vec3 matColor = pGeometry->GetColor(ray.primID, a, b);
		float reflectivity = pGeometry->GetMaterial()->GetReflectivity();
		glm::vec3 Norm = pGeometry->GetNormal(ray.primID, a, b);
		glm::vec3 intersectPos = pGeometry->GetPosition(ray.primID, a, b);
		for (RTLight *pLight : pScene->GetLightList())
		{
			glm::vec3 LightColor = pLight->GetLightColor(intersectPos);
			glm::vec3 LightDirection = pLight->GetLightDirection(intersectPos);
			glm::vec3 HalfVector = glm::normalize(LightDirection + -RayDirection);

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

				rtcOccluded(pScene->GetRTCScene(), ShadowRay);
				if (ShadowRay.geomID == RTC_INVALID_GEOMETRY_ID)
				{
					TotalColor += matColor * LightColor * nDotL;
				}
			}
		}

		m_pCanvas->WritePixel(x, y, GlmVec3ToRealArray(TotalColor));
	}
	else
	{
		glm::vec3 enviromentColor = pScene->GetEnvironmentMap()->GetColor(RayDirection);
		m_pCanvas->WritePixel(x, y, GlmVec3ToRealArray(enviromentColor));
	}
}


void RTRenderer::DrawScene(Camera *pCamera, Scene *pScene)
{
	RTScene *pRTScene = RT_RENDERER_CAST<RTScene*>(pScene);
	RTCamera *pRTCamera = RT_RENDERER_CAST<RTCamera*>(pCamera);

	pRTScene->PreDraw();

	UINT Width = pRTCamera->GetWidth();
	UINT Height = pRTCamera->GetHeight();

	const UINT THREAD_BLOCK_SIZE = 16;

	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	const UINT numWorkerThreads = sysinfo.dwNumberOfProcessors;

	if (m_pLastScene != pRTScene || pRTCamera != m_pLastCamera)
	{
		const UINT NumTasks = ceil(Height / (float)THREAD_BLOCK_SIZE) * ceil(Width / (float)THREAD_BLOCK_SIZE);
		m_ThreadArgs.reserve(NumTasks);
		for (UINT y = 0; y < Height; y += THREAD_BLOCK_SIZE)
		{
			for (UINT x = 0; x < Width; x += THREAD_BLOCK_SIZE)
			{
				m_ThreadArgs.push_back(RayTraceThreadArgs(
					this, 
					pRTScene, 
					pRTCamera, 
					PixelRange(x, y, std::min(Width - x, THREAD_BLOCK_SIZE), std::min(Height - y, THREAD_BLOCK_SIZE)),
					&m_RunningThreadCounter,
					m_TracingFinishedEvent));
			}
		}
		m_pLastScene = pRTScene;
		m_pLastCamera = pRTCamera;
	}

	m_RunningThreadCounter = m_ThreadArgs.size();
	std::vector<RayTraceThreadArgs *> ThreadArgs;
	std::vector<HANDLE>  hThreadArray;

	auto RenderPixelBatch = [](PTP_CALLBACK_INSTANCE, PVOID pThreadData, PTP_WORK) -> void {
		RayTraceThreadArgs *pArgs = (RayTraceThreadArgs *)pThreadData;
		pArgs->m_pRenderer->RenderPixelRange(
			&pArgs->m_PixelRange,
			pArgs->m_pCamera,
			pArgs->m_pScene);
		if (InterlockedDecrement(pArgs->m_pOngoingThreadCounter) == 0)
		{
			SetEvent(pArgs->m_TracingFinishedEvent);
		}
	};

#if RT_MULTITHREAD
	ResetEvent(m_TracingFinishedEvent);
	for (auto &args : m_ThreadArgs)
	{
		PTP_WORK work = CreateThreadpoolWork(RenderPixelBatch, &args, nullptr);
		SubmitThreadpoolWork(work);
	}
	WaitForSingleObject(m_TracingFinishedEvent, INFINITE);
	CloseThreadpoolCleanupGroupMembers(m_ThreadPoolCleanupGroup, true, nullptr);
#else
	RenderPixelBatch(&RayTraceThreadArgs(this, pRTScene, pRTCamera, PixelRange(0, 0, Width, Height)));
#endif
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

RTEnvironmentColor::RTEnvironmentColor(CreateEnvironmentColor *pCreateEnvironmentColor) :
	m_Color(RealArrayToGlmVec3(pCreateEnvironmentColor->m_Color)) {}

RTEnvironmentTextureCube::RTEnvironmentTextureCube(CreateEnvironmentTextureCube *pCreateEnvironmentTextureCube)
{
	m_TextureCube = RTTextureCube(pCreateEnvironmentTextureCube->m_TextureNames);
}

RTScene::RTScene(RTCDevice device, RTEnvironmentMap *pEnvironmentMap) :
	m_bSceneCommitted(false),
	m_pEnvironmentMap(pEnvironmentMap)
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
	m_VerticalFieldOfView(pCreateCameraDescriptor->m_VerticalFieldOfView)
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
	float FocalLength = 1.0f / tan(m_VerticalFieldOfView / 2.0f);
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
		glm::mat4 yawRotation = glm::rotate(yaw, glm::vec3(0, 1, 0));
		lookDir = yawRotation * lookDir;
	}

	m_LookAt = m_FocalPoint + glm::vec3(lookDir) * glm::length(m_LookAt - m_FocalPoint);
}

