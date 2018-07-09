#define HLSL
#include "RaytracingShared.h"
#include "ShaderUtil.h"

RaytracingAccelerationStructure Scene : register(t0, space0);
RWTexture2D<float4> RenderTarget : register(u0);

ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);
Texture2D<float4> EnvironmentMap : SRV_REGISTER(EnvironmentMapSRVRegister);

SamplerState LinearSampler: SAMPLER_REGISTER(LinearSamplerRegister);

Buffer<uint> IndexBuffer : SRV_REGISTER_SPACE(IndexBufferSRVRegister, LocalRootSignatureRegisterSpace);
ByteAddressBuffer AttributeBuffer : SRV_REGISTER_SPACE(AttributeBufferSRVRegister, LocalRootSignatureRegisterSpace);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
	float4 color;
};

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

	// Unproject the pixel coordinate into a ray.
	float4 world = mul(float4(screenPos, 0, 1), g_sceneCB.projectionToWorld);

	world.xyz /= world.w;
	origin = g_sceneCB.cameraPosition.xyz;
	direction = normalize(world.xyz - origin);
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
	RayPayload payload = { float4(0, 0, 0, 0) };
	TraceRay(Scene, 0, ~0, 0, 1, 0, ray, payload);

	// Write the raytraced color to the output texture.
	RenderTarget[DispatchRaysIndex()] = payload.color;
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

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
	uint3 indices = uint3(IndexBuffer[PrimitiveIndex() * 3], IndexBuffer[PrimitiveIndex() * 3 + 1], IndexBuffer[PrimitiveIndex() * 3 + 2]);
	payload.color = float4(GetAttributes(indices, attr.barycentrics).Normal, 1);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
	payload.color = SampleEnvironmentMap(WorldRayDirection());
}
