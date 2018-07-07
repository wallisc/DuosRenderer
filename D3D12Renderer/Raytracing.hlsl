#define HLSL
#include "RaytracingShared.h"

RaytracingAccelerationStructure Scene : register(t0, space0);
RWTexture2D<float4> RenderTarget : register(u0);

ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
	float4 color;
};

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
	TraceRay(Scene, 0, ~0, 0, 0, 0, ray, payload);

	// Write the raytraced color to the output texture.
	RenderTarget[DispatchRaysIndex()] = payload.color;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
	payload.color = float4(attr.barycentrics, 0, 1);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
	float4 background = float4(0.0f, 0.2f, 0.4f, 1.0f);
	payload.color = background;
}
