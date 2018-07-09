#pragma once

// Global Root Signature
#define EnvironmentMapSRVRegister 1
#define LinearSamplerRegister 0


// Local Root Signature
#define LocalRootSignatureRegisterSpace 1
#define IndexBufferSRVRegister 0
#define AttributeBufferSRVRegister 1

#ifndef HLSL
struct float2
{
	float   x, y;
};

struct float3
{
	float   x, y, z;
};

struct float4
{
	float   x, y, z, w;
};

struct float4x4
{
	float4 v0, v1, v2, v3;
};
#endif

struct VertexAttribute
{
	float3 Normal;
	float3 Tangent;
	float2 UV;
};

struct SceneConstantBuffer
{
	float4x4 projectionToWorld;
	float4 cameraPosition;
};