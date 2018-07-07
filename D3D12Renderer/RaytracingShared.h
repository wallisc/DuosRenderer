#pragma once
#define EnvironmentMapSRVRegister 1
#define LinearSamplerRegister 0

#ifndef HLSL
struct float4
{
	float   x, y, z, w;
};

struct float4x4
{
	float4 v0, v1, v2, v3;
};
#endif

struct SceneConstantBuffer
{
	float4x4 projectionToWorld;
	float4 cameraPosition;
};