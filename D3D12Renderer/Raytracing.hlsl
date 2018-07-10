#define HLSL
#include "RaytracingShared.h"
#include "ShaderUtil.h"


RaytracingAccelerationStructure Scene : register(t0, space0);
RWTexture2D<float4> RenderTarget : register(u0);

ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);
Texture2D<float4> EnvironmentMap : SRV_REGISTER(EnvironmentMapSRVRegister);

SamplerState LinearSampler: SAMPLER_REGISTER(LinearSamplerRegister);
SamplerState PointSampler: SAMPLER_REGISTER(PointSamplerRegister);

Buffer<uint> IndexBuffer : SRV_REGISTER_SPACE(IndexBufferSRVRegister, LocalRootSignatureRegisterSpace);
ByteAddressBuffer AttributeBuffer : SRV_REGISTER_SPACE(AttributeBufferSRVRegister, LocalRootSignatureRegisterSpace);
Texture2D<float4> DiffuseTexture : SRV_REGISTER_SPACE(DiffuseTextureSRVRegister, LocalRootSignatureRegisterSpace);

ConstantBuffer<MaterialConstants> Material : CONSTANT_REGISTER_SPACE(MaterialCBVRegister, LocalRootSignatureRegisterSpace);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;


struct OcclusionPayload
{
	uint HitFound;
};

static uint seed = 0;
float rand() { return frac(sin(DispatchRaysIndex().x + DispatchRaysIndex().y * DispatchRaysDimensions().x + seed++ + g_sceneCB.time)*43758.5453123); }

float4 SampleEnvironmentMap(float3 dir)
{
	float2 uv;
	float p = atan2(dir.z, dir.x);
	p = p > 0 ? p : p + 2 * 3.14;
	uv.x = p / (2 * 3.14);

	uv.y = acos(dir.y) / (3.14);
	return EnvironmentMap.SampleLevel(LinearSampler, uv, 0);
}

// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
inline void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
	float2 xy = index + 0.5f; // center in the middle of the pixel.
	float2 screenPos = xy / DispatchRaysDimensions() * 2.0 - 1.0;

	// Invert Y for DirectX-style coordinates.
	screenPos.y = -screenPos.y;

	float2 pixelDimensions = 1.0 / DispatchRaysDimensions();
	float2 jitter = pixelDimensions * (float2(rand(), rand()) * 2.0 - 1.0);
	screenPos += jitter;

	// Unproject the pixel coordinate into a ray.
	float4 world = mul(float4(screenPos, 0, 1), g_sceneCB.projectionToWorld);

	world.xyz /= world.w;
	origin = g_sceneCB.cameraPosition.xyz;
	direction = normalize(world.xyz - origin);
}

float3 GenerateCosineWeightedRay()
{
	float u1 = rand(), u2 = rand();
	const float r = sqrt(u1);
	const float theta = 2 * 3.14 * u2;

	const float x = r * cos(theta);
	const float y = r * sin(theta);

	return float3(x, y, sqrt(max(0.0f, 1 - u1)));
}

float3 GenerateRandomDirection(float3 normal)
{
	// Uniform hemisphere sampling from: http://www.rorydriscoll.com/2009/01/07/better-sampling/
	float u1 = rand(), u2 = rand();
	float r = sqrt(1.0 - u1 * u1);
	float phi = 2.0 * 3.14 * u2;

	float3 direction = float3(cos(phi) * r, sin(phi) * r, u1);
	if (dot(normal, direction) > 0.0)
	{
		return direction;
	}
	else
	{
		return -direction;
	}
}

[shader("raygeneration")]
void MyRaygenShader()
{
	float3 rayDir;
	float3 origin;

	// Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
	GenerateCameraRay(DispatchRaysIndex(), origin, rayDir);

	// Trace the ray.
	// Set the ray's extents.
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = rayDir;
	// Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
	// TMin should be kept small to prevent missing geometry at close contact areas.
	ray.TMin = 0.001;
	ray.TMax = 10000.0;
	LightPayload payload = { float4(0, 0, 0, 0), 1 };
	TraceRay(Scene, 
		0, 
		~0, 
		0, 
		ShaderRecordsPerGeometry,
		LightingMissShaderRecordIndex, 
		ray, 
		payload);

	// Write the raytraced color to the output texture.
	RenderTarget[DispatchRaysIndex()] += payload.Color;
}

VertexAttribute GetVertexAttribute(uint index)
{
	// Appears to be a bug with structured buffer, working around it
	// using a ByteAddressBuffer for now
	VertexAttribute attr;
	attr.Normal = asfloat(AttributeBuffer.Load3(index * 32));
	attr.Tangent = asfloat(AttributeBuffer.Load3(index * 32 + 12));
	attr.UV = asfloat(AttributeBuffer.Load2(index * 32 + 24));
	return attr;
}

float3 CalculateValueFromBarycentrics(float3 v0, float3 v1, float3 v2, float2 barycentrics)
{
	return v0 + barycentrics.x * (v1 - v0) + barycentrics.y * (v2 - v0);
}

float2 CalculateValueFromBarycentrics(float2 v0, float2 v1, float2 v2, float2 barycentrics)
{
	return v0 + barycentrics.x * (v1 - v0) + barycentrics.y * (v2 - v0);
}

VertexAttribute GetAttributes(uint3 indicies, float2 barycentrics)
{
	VertexAttribute v0 = GetVertexAttribute(indicies.x);
	VertexAttribute v1 = GetVertexAttribute(indicies.y);
	VertexAttribute v2 = GetVertexAttribute(indicies.z);

	VertexAttribute hitAttributes;
	hitAttributes.Normal = normalize(CalculateValueFromBarycentrics(v0.Normal, v1.Normal, v2.Normal, barycentrics));
	hitAttributes.Tangent = normalize(CalculateValueFromBarycentrics(v0.Tangent, v1.Tangent, v2.Tangent, barycentrics));
	hitAttributes.UV = CalculateValueFromBarycentrics(v0.UV, v1.UV, v2.UV, barycentrics);

	return hitAttributes;
}

float4 GetDiffuseColor(VertexAttribute attributes)
{
	if (Material.HasDiffuseTexture)
	{
		return DiffuseTexture.SampleLevel(PointSampler, attributes.UV, 0);
	}
	else
	{
		return Material.DiffuseColor;
	}
}

float3 HitPosition()
{
	return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

static float Fresnel_ShlicksApproximation(float3 Normal, float3 LightRay, float indexOfRefraction)
{
	float reflectionCoefficient = (indexOfRefraction - 1.0) * (indexOfRefraction - 1.0) / ((indexOfRefraction + 1.0) * (indexOfRefraction + 1.0));
	return (reflectionCoefficient + (1.0 - reflectionCoefficient) * pow(1.0f - saturate(dot(LightRay, Normal)), 5));
}

float4 GetLightColor(float3 origin, float3 direction, uint recursionLevel)
{
	if (recursionLevel + 1 <= MaxLevelsOfRecursion)
	{
		LightPayload lightSourcePayload = { float4(0, 0, 0, 0), recursionLevel + 1 };
		{
			RayDesc lightRay;
			lightRay.Origin = origin;
			lightRay.Direction = direction;
			lightRay.TMin = 0.001;
			lightRay.TMax = 10000.0;
			TraceRay(Scene,
				RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
				DefaultInstanceMask,
				0,
				ShaderRecordsPerGeometry,
				LightingMissShaderRecordIndex,
				lightRay,
				lightSourcePayload);
		}
		return lightSourcePayload.Color;
	}
	else
	{
		return SampleEnvironmentMap(direction);
	}
}

[shader("closesthit")]
void MyClosestHitShader(inout LightPayload payload, in MyAttributes attr)
{
	uint3 indices = uint3(IndexBuffer[PrimitiveIndex() * 3], IndexBuffer[PrimitiveIndex() * 3 + 1], IndexBuffer[PrimitiveIndex() * 3 + 2]);
	VertexAttribute attributes = GetAttributes(indices, attr.barycentrics);
	float percentVisible = 1.0;

#if 1
	// Ambient occlusion is expensive, so only do it on the first pass
	const bool doAmbientOcclusion = payload.RecursionLevel == 1;
	if (doAmbientOcclusion)
	{
		const uint numOcclusionRaysToFire = 4;
		for (uint i = 0; i < numOcclusionRaysToFire; i++)
		{
			RayDesc occlusionRay;
			occlusionRay.Origin = HitPosition();
			occlusionRay.Direction = normalize(attributes.Normal + float3(rand(), rand(), rand()));
			occlusionRay.TMin = 0.001;
			occlusionRay.TMax = 10000.0;
			OcclusionPayload occlusionPayload = { true };
			TraceRay(Scene,
				RAY_FLAG_CULL_BACK_FACING_TRIANGLES | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
				DefaultInstanceMask,
				0,
				ShaderRecordsPerGeometry,
				OcclusionMissShaderRecordIndex,
				occlusionRay,
				occlusionPayload);

			if (occlusionPayload.HitFound)
			{
				percentVisible -= 1.0 / (float)(numOcclusionRaysToFire * 2);
			}
		}
	}
#endif

	float4 reflectedlightColor = float4(0, 0, 0, 0);
	float3 reflectionRayDirection = reflect(WorldRayDirection(), attributes.Normal);
	reflectionRayDirection = normalize(reflectionRayDirection + 0.05 * float3(rand(), rand(), rand()));
	float reflectivity = Fresnel_ShlicksApproximation(attributes.Normal, reflectionRayDirection, Material.IndexOfRefraction);
	reflectivity = reflectivity;
	if (reflectivity > 0.001)
	{
		reflectedlightColor = GetLightColor(HitPosition(), reflectionRayDirection, payload.RecursionLevel);
	}
	
	float3 lightDirection = normalize(attributes.Normal + float3(rand(), rand(), rand()));
	float4 lightColor = GetLightColor(HitPosition(), lightDirection, payload.RecursionLevel);
	float4 diffuseContibution = dot(attributes.Normal, lightDirection) * GetDiffuseColor(attributes) * lightColor;

	payload.Color = lerp(diffuseContibution, reflectedlightColor, reflectivity) * percentVisible;
}

[shader("miss")]
void LightingMiss(inout LightPayload payload)
{
	payload.Color = SampleEnvironmentMap(WorldRayDirection());
}

[shader("miss")]
void OcclusionMiss(inout OcclusionPayload payload)
{
	payload.HitFound = false;
}
