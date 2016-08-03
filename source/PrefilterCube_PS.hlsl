#include "PrefilterCube.h"
#include "ShaderUtil.h"

#define NUM_SAMPLES 256

TextureCube EnvironmentMap : register(t0);
sampler EnvironmentMapSampler : register(s0);
cbuffer cbMaterial : register(b0)
{
    float Roughness;
};

float rand(in float2 uv)
{
    float2 noise = (frac(sin(dot(uv, float2(12.9898, 78.233)*2.0)) * 43758.5453));
    return abs(noise.x + noise.y) * 0.5;
}

float4 main(PREFILTERED_CUBE_VS_OUTPUT input) : SV_TARGET
{
    float TotalWeight = 0.0f;
    float3 N = normalize(input.Norm);
    float3 V = N;
    float3 PrefilteredColor = 0;
    const uint NumSamples = 256;
    for (uint i = 0; i < NumSamples; i++)
    {
        float2 Xi = Hammersley(i, NumSamples);
        float3 H = ImportanceSampleGGX(Xi, Roughness, N);
        float3 L = 2 * dot(V, H) * H - V;
        float NoL = saturate(dot(N, L));
        if (NoL > 0)
        {
            PrefilteredColor += EnvironmentMap.Sample(EnvironmentMapSampler, L).rgb * NoL;
            TotalWeight += NoL;
        }
    }
    return float4(PrefilteredColor / TotalWeight, 1.0f);
}
