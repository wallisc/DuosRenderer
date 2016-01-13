#include "RTRenderer.h"
#include "RendererException.h"

#include <atlbase.h>
#include <queue>

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
		uv = glm::vec2(1.0f  - (dir.x / dir.y + 1.0f) * 0.5,
			(dir.z / dir.y + 1.0f) * 0.5);
		break;
	case NEG_Y:
		uv = glm::vec2((dir.x / dir.y + 1.0f) * 0.5,
			(dir.z / dir.y + 1.0f) * 0.5);
		break;
	case POS_Z:
		uv = glm::vec2((dir.x / dir.z + 1.0f) * 0.5,
			1.0f - (dir.y / dir.z + 1.0f) * 0.5);
		break;
	case NEG_Z:
		uv = glm::vec2((dir.x / dir.z + 1.0f) * 0.5,
			(dir.y / dir.z + 1.0f) * 0.5);
		break;
	default:
		assert(false);
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
	m_Roughness = pCreateMaterialDescriptor->m_Roughness;
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

RTRenderer::RTRenderer(unsigned int width, unsigned int height) :
	m_LastRenderSettings(DefaultRenderSettings)
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

RayBatch::RayBatch(RTCScene Scene, _In_reads_(NumRays) const glm::vec3 *RayOrigins, _In_reads_(NumRays) const glm::vec3 *RayDirections, unsigned int NumRays) :
m_NumRays(NumRays)
{
	if (NumRays == 1)
	{
		Ray = {};
		Ray.tnear = 0.0f;
		Ray.tfar = FLT_MAX;
		Ray.geomID = RTC_INVALID_GEOMETRY_ID;
		Ray.primID = RTC_INVALID_GEOMETRY_ID;
		Ray.mask = -1;
		Ray.time = 0;
		memcpy(Ray.org, RayOrigins, sizeof(*RayOrigins));
		memcpy(Ray.dir, RayDirections, sizeof(*RayDirections));

		rtcIntersect(Scene, Ray);
	}
	else if (NumRays <= 4)
	{
		InitRayStruct(Ray4, RayOrigins, RayDirections, NumRays);
		rtcIntersect4(&ValidMask, Scene, Ray4);
	}
	else if (NumRays <= 8)
	{
		InitRayStruct(Ray8, RayOrigins, RayDirections, NumRays);
		rtcIntersect8(&ValidMask, Scene, Ray8);
	}
	else
	{
		InitRayStruct(Ray16, RayOrigins, RayDirections, NumRays);
		rtcIntersect16(&ValidMask, Scene, Ray16);
	}
}

unsigned int RayBatch::GetGeometryID(unsigned int RayIndex)
{
	assert(RayIndex < m_NumRays);
	if (m_NumRays == 1)
	{
		return Ray.geomID;
	}
	else if (m_NumRays <= 4)
	{
		return GetGeometryIDInternal(Ray4, RayIndex);
	}
	else if (m_NumRays <= 8)
	{
		return GetGeometryIDInternal(Ray8, RayIndex);
	}
	else
	{
		return GetGeometryIDInternal(Ray16, RayIndex);
	}
}

unsigned int RayBatch::GetPrimID(unsigned int RayIndex)
{
	assert(RayIndex < m_NumRays);
	if (m_NumRays == 1)
	{
		return Ray.primID;
	}
	else if (m_NumRays <= 4)
	{
		return GetPrimIDInternal(Ray4, RayIndex);
	}
	else if (m_NumRays <= 8)
	{
		return GetPrimIDInternal(Ray8, RayIndex);
	}
	else
	{
		return GetPrimIDInternal(Ray16, RayIndex);
	}
}

glm::vec3 RayBatch::GetBaryocentricCoordinate(unsigned int RayIndex)
{
	assert(RayIndex < m_NumRays);
	float u, v;
	if (m_NumRays == 1)
	{
		u = Ray.u;
		v = Ray.v;
	}
	else if (m_NumRays <= 4)
	{
		GetBaryocentricCoordinateInternal(Ray4, RayIndex, u, v);
	}
	else if (m_NumRays <= 8)
	{
		GetBaryocentricCoordinateInternal(Ray8, RayIndex, u, v);
	}
	else
	{
		GetBaryocentricCoordinateInternal(Ray16, RayIndex, u, v);
	}
	return glm::vec3(u, v, 1.0 - u - v);
}

void GammaCorrect(glm::vec3 &Color)
{
	const float gammaCurve = 1.0 / 2.2;
	Color = glm::pow(Color, glm::vec3(gammaCurve, gammaCurve, gammaCurve));
}

void RTRenderer::RenderPixelRange(PixelRange *pRange, RTCamera *pCamera, RTScene *pScene, const RenderSettings &RenderFlags)
{
	assert(pRange->m_Width > 0 && pRange->m_X + pRange->m_Width <= pCamera->GetWidth());
	assert(pRange->m_Height > 0 && pRange->m_Y + pRange->m_Height <= pCamera->GetHeight());

	const UINT INTERSECT_BLOCK_WIDTH = 2;
	const UINT INTERSECT_BLOCK_HEIGHT = 2;
	const UINT RAYS_PER_BLOCK = INTERSECT_BLOCK_HEIGHT * INTERSECT_BLOCK_WIDTH;

	const unsigned int Width = pCamera->GetWidth();
	const unsigned int Height = pCamera->GetHeight();
	const glm::vec3 FocalPoint = pCamera->GetFocalPoint();
	const float PixelWidth = pCamera->GetLensWidth() / Width;
	const float PixelHeight = pCamera->GetLensHeight() / Height;
	const glm::vec3 right = pCamera->GetRight();

	for (UINT topLeftX = pRange->m_X; topLeftX < pRange->m_X + pRange->m_Width; topLeftX += INTERSECT_BLOCK_WIDTH)
	{
		for (UINT topLeftY = pRange->m_Y; topLeftY < pRange->m_Y + pRange->m_Height; topLeftY += INTERSECT_BLOCK_HEIGHT)
		{
			glm::vec3 LensPoints[RAYS_PER_BLOCK];
			glm::vec3 RayDirections[RAYS_PER_BLOCK];
			glm::vec3 Colors[RAYS_PER_BLOCK];

			UINT rayIndex = 0;
			const UINT BlockWidth = min(INTERSECT_BLOCK_WIDTH, (pRange->m_X + pRange->m_Width) - topLeftX);
			const UINT BlockHeight = min(INTERSECT_BLOCK_HEIGHT, (pRange->m_Y + pRange->m_Height) - topLeftY);
			const UINT BlockSize = BlockWidth * BlockHeight;
			for (UINT xOffset = 0; xOffset < BlockWidth; xOffset++)
			{
				for (UINT yOffset = 0; yOffset < BlockHeight; yOffset++)
				{
					UINT x = topLeftX + xOffset;
					UINT y = topLeftY + yOffset;

					glm::vec3 coord(pCamera->GetLensWidth() * (float)x / (float)Width, pCamera->GetLensHeight() * (float)(Height - y) / (float)Height, 0.0f);
					coord -= glm::vec3(pCamera->GetLensWidth() / 2.0f, pCamera->GetLensHeight() / 2.0f, 0.0f);
					coord += glm::vec3(PixelWidth / 2.0f, -PixelHeight / 2.0f, 0.0f); // Make sure the ray is centered in the pixel

					LensPoints[rayIndex] = pCamera->GetLensPosition() + coord.y * pCamera->GetUp() + coord.x * right;
					RayDirections[rayIndex] = glm::normalize(LensPoints[rayIndex] - FocalPoint);
					rayIndex++;
				}
			}

			Trace(pScene, LensPoints, RayDirections, Colors, BlockSize, ShadePixelRecursionInfo());

			rayIndex = 0;
			for (UINT xOffset = 0; xOffset < BlockWidth; xOffset++)
			{
				for (UINT yOffset = 0; yOffset < BlockHeight; yOffset++)
				{
					UINT x = topLeftX + xOffset;
					UINT y = topLeftY + yOffset;
					if (RenderFlags.m_GammaCorrection)
					{
						GammaCorrect(Colors[rayIndex]);
					}
					m_pCanvas->WritePixel(x, y, GlmVec3ToRealArray(Colors[rayIndex]));
					rayIndex++;
				}
			}
		}
	}
}

void RTRenderer::Trace(_In_ RTScene *pScene, _In_reads_(NumRays) const glm::vec3 *pRayOrigins, _In_reads_(NumRays) const glm::vec3 *pRayDirs, _Out_ glm::vec3 *pColors, UINT NumRays, ShadePixelRecursionInfo &RecursionInfo)
{
	const UINT NumBatches = (NumRays - 1)/ RAYS_PER_INTERSECT_BATCH + 1;
	for (UINT RayBatchIndex = 0; RayBatchIndex < NumBatches; RayBatchIndex++)
	{
		const UINT BatchSize = (RayBatchIndex < NumBatches - 1 || NumRays % RAYS_PER_INTERSECT_BATCH == 0) ? RAYS_PER_INTERSECT_BATCH : NumRays % RAYS_PER_INTERSECT_BATCH;
		assert(IsIntersectCountEmbreeCompatible(BatchSize)); 

		RayBatch RayBatch(pScene->GetRTCScene(), &pRayOrigins[RayBatchIndex * RAYS_PER_INTERSECT_BATCH], &pRayDirs[RayBatchIndex * RAYS_PER_INTERSECT_BATCH], BatchSize);
		for (UINT RayIndex = 0; RayIndex < BatchSize; RayIndex++)
		{
			pColors[RayIndex] = ShadePixel(
				pScene,
				RayBatch.GetPrimID(RayIndex),
				pScene->GetRTGeometry(RayBatch.GetGeometryID(RayIndex)),
				RayBatch.GetBaryocentricCoordinate(RayIndex),
				-pRayDirs[RayBatchIndex * RAYS_PER_INTERSECT_BATCH + RayIndex],
				RecursionInfo);
		}
	}
}

float FltRand()
{
	return rand() / (float)RAND_MAX;
}

glm::vec3 cosineWeightedSample(glm::vec3 normal, float rand1, float rand2) {
   float theta = acos(sqrt(rand2)); 
   float phi = M_PI * 2.0f * rand1; 

   float xs = sin(theta) * cos(phi), ys = cos(theta), zs = sin(theta) * sin(phi);  

   glm::vec3 y(normal); 
   glm::vec3 h(normal); 
   if (abs(h.x) <= abs(h.y) && abs(h.x) <= abs(h.z)) h.x = 1.0f; 
   else if (abs(h.y) <= abs(h.x) && abs(h.y) <= abs(h.z)) h.y = 1.0f; 
   else h.z = 1.0f; 

   glm::vec3 x = glm::normalize(glm::cross(h, y));
   glm::vec3 z = glm::normalize(glm::cross(x, y));
   glm::vec3 dir = xs * x + ys * y + zs * z; 
    
   return glm::normalize(dir); 
} 


glm::vec3 RTRenderer::ShadePixel(RTScene *pScene, unsigned int primID, RTGeometry *pGeometry, glm::vec3 baryocentricCoord, glm::vec3 ViewVector, ShadePixelRecursionInfo &RecursionInfo)
{
	if (pGeometry)
	{
		assert(pGeometry != nullptr);

		glm::vec3 TotalDiffuse = glm::vec3(0.0f);
		glm::vec3 TotalSpecular = glm::vec3(0.0f);
		float TotalFresnel = 0.0f;

		glm::vec3 matColor = pGeometry->GetColor(primID, baryocentricCoord.x, baryocentricCoord.y);
		float reflectivity = pGeometry->GetRTMaterial()->GetReflectivity();
		float Roughness = pGeometry->GetRTMaterial()->GetRoughness();
		glm::vec3 Norm = pGeometry->GetNormal(primID, baryocentricCoord.x, baryocentricCoord.y);
		glm::vec3 intersectPos = pGeometry->GetPosition(primID, baryocentricCoord.x, baryocentricCoord.y);
		for (RTLight *pLight : pScene->GetLightList())
		{
			glm::vec3 LightColor = pLight->GetLightColor(intersectPos);
			glm::vec3 LightDirection = pLight->GetLightDirection(intersectPos);
			glm::vec3 HalfVector = glm::normalize(LightDirection + ViewVector);

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
					TotalDiffuse += matColor * LightColor * nDotL;

					float fresnel;
					TotalSpecular += LightColor * CookTorrance().BRDF(ViewVector, Norm, LightDirection, Roughness, reflectivity, fresnel);
					TotalFresnel += fresnel;
				}
			}
		}

		glm::vec3 ReflectionColor = glm::vec3(0.0f);
		bool bGetReflectionFromEnvironmentMap = RecursionInfo.m_NumRecursions < MAX_RAY_RECURSION;
		glm::vec3 ReflOrigin = intersectPos + Norm * LARGE_EPSILON; //Offset a small amount to avoid self-intersection
		glm::vec3 ReflectionVector = glm::reflect(-ViewVector, Norm);
		if (bGetReflectionFromEnvironmentMap)
		{
			if (m_bEnableMultiRayEmission && RecursionInfo.m_NumRecursions < 2)
			{
				glm::vec3 Colors[RAYS_PER_INTERSECT_BATCH];
				float BRDFValues[RAYS_PER_INTERSECT_BATCH];
				glm::vec3 ReflectionVectors[RAYS_PER_INTERSECT_BATCH];
				glm::vec3 ReflectionOrigins[RAYS_PER_INTERSECT_BATCH];
				const UINT NumBatches = (RAYS_PER_INTERSECT_BATCH - 1) / RAY_EMISSION_COUNT + 1;

				std::fill(ReflectionOrigins, ReflectionOrigins + RAYS_PER_INTERSECT_BATCH, ReflOrigin);
						
				UINT NumRaysBatched = 0;
				UINT NumSamplesTaken = 0;
				for (UINT RayIndex = 0; RayIndex < RAY_EMISSION_COUNT; RayIndex++)
				{
					// Make sure the reflection vector is tested
					ReflectionVectors[NumRaysBatched] = (RayIndex == 0) ? ReflectionVector : cosineWeightedSample(ReflectionVector, FltRand(), FltRand());
					float fresnel;
					const float BRDFValue = CookTorrance().BRDF(ViewVector, Norm, ReflectionVectors[NumRaysBatched], Roughness, reflectivity, fresnel);
					if (RecursionInfo.m_TotalContribution * BRDFValue > MEDIUM_EPSILON)
					{
						BRDFValues[NumRaysBatched] = BRDFValue;
						NumRaysBatched++;
						if (NumRaysBatched == RAYS_PER_INTERSECT_BATCH || RayIndex == RAY_EMISSION_COUNT - 1)
						{
							NumRaysBatched = TruncateToCompatibleEmbreeCompatible(NumRaysBatched);
							Trace(pScene, ReflectionOrigins, ReflectionVectors, Colors, NumRaysBatched, ShadePixelRecursionInfo(RecursionInfo.m_NumRecursions + 1, RecursionInfo.m_TotalContribution * BRDFValue));
							for (UINT ColorIndex = 0; ColorIndex < NumRaysBatched; ColorIndex++)
							{
								ReflectionColor += Colors[ColorIndex] * BRDFValues[ColorIndex];
							}
							NumRaysBatched = 0;
						}
						TotalFresnel += fresnel;
						NumSamplesTaken++;
					}
				}

				TotalFresnel /= NumSamplesTaken + 1;
				ReflectionColor /= NumSamplesTaken + 1;
			}
			else
			{
				float fresnel;
				float BRDFValue = CookTorrance().BRDF(ViewVector, Norm, ReflectionVector, Roughness, reflectivity, fresnel);
				if (RecursionInfo.m_TotalContribution * BRDFValue > MEDIUM_EPSILON)
				{
					Trace(pScene, &ReflOrigin, &ReflectionVector, &ReflectionColor, 1, ShadePixelRecursionInfo(RecursionInfo.m_NumRecursions + 1, RecursionInfo.m_TotalContribution * BRDFValue));
				}
				ReflectionColor *= BRDFValue;
				TotalFresnel += fresnel;
				TotalFresnel /= 2.0f;
			}
		}
		else
		{
			ReflectionColor = pScene->GetEnvironmentMap()->GetColor(ReflectionVector);
		}
		
		TotalSpecular += ReflectionColor;


		return glm::clamp(TotalDiffuse * (1.0f - TotalFresnel) + TotalSpecular, glm::vec3(0.0f), glm::vec3(1.0f));
		//return glm::clamp(TotalFresnel * TotalSpecular, glm::vec3(0.0f), glm::vec3(1.0f));
	}
	else
	{
		return pScene->GetEnvironmentMap()->GetColor(-ViewVector);
	}
}

Geometry *RTRenderer::GetGeometryAtPixel(Camera *pCamera, Scene *pScene, Vec2 PixelCoord)
{
	RTScene *pRTScene = RT_RENDERER_CAST<RTScene*>(pScene);
	RTCamera *pRTCamera = RT_RENDERER_CAST<RTCamera*>(pCamera);

	pRTScene->PreDraw();
	const UINT Width = pRTCamera->GetWidth();
	const UINT Height = pRTCamera->GetHeight();

	const float PixelWidth = pRTCamera->GetLensWidth() / Width;
	const float PixelHeight = pRTCamera->GetLensHeight() / Height;

	glm::vec3 coord(pRTCamera->GetLensWidth() * PixelCoord.x / (float)Width, pRTCamera->GetLensHeight() * (float)(Height - PixelCoord.y) / (float)Height, 0.0f);
	coord -= glm::vec3(pRTCamera->GetLensWidth() / 2.0f, pRTCamera->GetLensHeight() / 2.0f, 0.0f);
	coord += glm::vec3(PixelWidth / 2.0f, -PixelHeight / 2.0f, 0.0f); // Make sure the ray is centered in the pixel

	const glm::vec3 LensPoints = pRTCamera->GetLensPosition() + coord.y * pRTCamera->GetUp() + coord.x * pRTCamera->GetRight();
	const glm::vec3 RayDirections = glm::normalize(LensPoints - pRTCamera->GetFocalPoint());

	RayBatch batch(pRTScene->GetRTCScene(), &LensPoints, &RayDirections, 1);

	unsigned int meshID = batch.GetGeometryID(0);
	return  meshID == RTC_INVALID_GEOMETRY_ID ? nullptr : pRTScene->GetRTGeometry(meshID);;
}



void RTRenderer::DrawScene(Camera *pCamera, Scene *pScene, const RenderSettings &RenderFlags)
{
	RTScene *pRTScene = RT_RENDERER_CAST<RTScene*>(pScene);
	RTCamera *pRTCamera = RT_RENDERER_CAST<RTCamera*>(pCamera);

#if RT_AVOID_RENDERING_REDUNDANT_FRAMES
	// TODO: Also need to ensure the canvas hasn't changed
	// If both the camera and scene haven't changed, don't re-render the scene
	if (pRTScene->GetVersionID() == m_LastSceneID && pRTCamera->GetVersionID() == m_LastCameraVersionID && m_LastRenderSettings == RenderFlags)
	{
		return;
	}
#endif

	m_LastSceneID = pRTScene->GetVersionID();
	m_LastCameraVersionID = pRTCamera->GetVersionID();
	m_LastRenderSettings = RenderFlags;

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
					RenderFlags,
					PixelRange(x, y, min(Width - x, THREAD_BLOCK_SIZE), min(Height - y, THREAD_BLOCK_SIZE)),
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
			pArgs->m_pScene,
			pArgs->m_RenderFlags);
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
	m_scene = rtcDeviceNewScene(device, cSceneFlags, RTC_INTERSECT1 | RTC_INTERSECT4 | RTC_INTERSECT8 | RTC_INTERSECT16);
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

	pRTGeometry->RegisterObserver(this);
	pRTGeometry->GetRTMaterial()->RegisterObserver(this);
	NotifyChanged();
}

void RTScene::AddLight(_In_ Light *pLight)
{
	RTLight *pRTLight = static_cast<RTLight*>(pLight);
	m_LightList.push_back(pRTLight);

	pRTLight->RegisterObserver(this);
	NotifyChanged();
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
	return glm::normalize(m_vertexData[i0].m_norm * gamma +
		m_vertexData[i1].m_norm * alpha +
		m_vertexData[i2].m_norm * beta);
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

	NotifyChanged();
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
		m_Up = glm::vec3(yawRotation * glm::vec4(m_Up, 0.0f));
		lookDir = yawRotation * lookDir;
	}

	m_LookAt = m_FocalPoint + glm::vec3(lookDir) * glm::length(m_LookAt - m_FocalPoint);
	NotifyChanged();
}

float CookTorrance::BRDF(_In_ const glm::vec3 &ViewVector, _In_ const glm::vec3 &Normal, _In_ const glm::vec3 &IncomingRadianceVector, _In_ float Roughness, _In_ float BaseReflectivity, _Out_ float &FresnelFactor)
{
	glm::vec3 HalfwayVector = glm::normalize(ViewVector + IncomingRadianceVector);
	float nDotL = saturate(glm::dot(Normal, IncomingRadianceVector));
	float nDotV = saturate(glm::dot(Normal, ViewVector));

	FresnelFactor = F(Normal, HalfwayVector, BaseReflectivity);
	float Distribution = D(Normal, HalfwayVector, Roughness);
	float GeometricAttenuation = G(ViewVector, Normal, IncomingRadianceVector, Roughness);
	return FresnelFactor * Distribution * GeometricAttenuation / (4.0 * nDotL * nDotV);
}
