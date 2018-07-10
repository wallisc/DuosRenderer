#pragma once

// Global Root Signature
#define EnvironmentMapSRVRegister 1
#define LinearSamplerRegister 0
#define PointSamplerRegister 1


// Local Root Signature
#define LocalRootSignatureRegisterSpace 1
#define IndexBufferSRVRegister 0
#define AttributeBufferSRVRegister 1
#define DiffuseTextureSRVRegister 2

#define MaterialCBVRegister 0

#define ShaderRecordsPerGeometry 1

#define LightingMissShaderRecordIndex 0
#define OcclusionMissShaderRecordIndex 1
#define NumberOfMissShaders 2

#define MaxLevelsOfRecursion 2

#define DefaultInstanceMask 0xff

#ifndef HLSL
typedef UINT uint;
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

struct MaterialConstants
{
	float Roughness;
	float IndexOfRefraction;
	float Padding;
	uint HasDiffuseTexture;
	float4 DiffuseColor;
};

struct SceneConstantBuffer
{
	float4x4 projectionToWorld;
	float4 cameraPosition;
	uint time;
};

struct LightPayload
{
	float4 Color;
	uint RecursionLevel;
};