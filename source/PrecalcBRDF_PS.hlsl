#include "PrecalcBRDF.h"
#define NUM_SAMPLES 256
#define M_PI 3.14

TextureCube EnvironmentMap : register(t0);
sampler EnvironmentMapSampler : register(s0);
cbuffer cbMaterial : register(b0)
{
    float4 Roughness;
};

float rand(in float2 uv)
{
    float2 noise = (frac(sin(dot(uv, float2(12.9898, 78.233)*2.0)) * 43758.5453));
    return abs(noise.x + noise.y) * 0.5;
}

float3 cosineWeightedSample(float3 normal, float2 rand) {
    float theta = acos(sqrt(rand.x));
    float phi = M_PI * 2.0f * rand.y;

    const float sinTheta = sin(theta);
    const float sinPhi = sin(phi);
    const float cosPhi = cos(phi);
    const float cosTheta = cos(theta);

    float xs = sinTheta * cosPhi, ys = cosTheta, zs = sinTheta * sinPhi;

    float3 y = normal;
    float3 h = normal;
    if (abs(h.x) <= abs(h.y) && abs(h.x) <= abs(h.z)) h.x = 1.0f;
    else if (abs(h.y) <= abs(h.x) && abs(h.y) <= abs(h.z)) h.y = 1.0f;
    else h.z = 1.0f;

    float3 x = normalize(cross(h, y));
    float3 z = normalize(cross(x, y));
    float3 dir = xs * x + ys * y + zs * z;

    return normalize(dir);
}

float4 main(PRECALC_BRDF_VS_OUTPUT input) : SV_TARGET
{
    return float4(EnvironmentMap.Sample(EnvironmentMapSampler, input.Norm).xyz, 1.0f);
#if 0
    float3 TotalColor = float3(0, 0, 0);
    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        // TODO: This rand really sucks
        float3 SampleRay = cosineWeightedSample(ViewDirection, rand(input.Pos + i * float2(1.0, 1.0)));
        TotalColor += EnvironmentMap.Sample(EnvironmentMapSampler, SampleRay);
    }
    return float4(TotalColor / NUM_SAMPLES, 1.0f);
#endif
}