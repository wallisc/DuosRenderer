#include "PrecalcBRDF.h"
#include "ShaderUtil.h"

#define SQRT_2_DIVIDED_BY_PI 0.7978845608

float Schlicks_PartialGeometricAttenuation(float3 n, float3 v, float k)
{
    float nDotV = saturate(dot(n, v));
    return nDotV / (nDotV * (1 - k) + k);
}

float Schlicks_GeometricAttenuation(float3 v, float3 l, float3 n, float roughness)
{
    float k = roughness * SQRT_2_DIVIDED_BY_PI;
    return Schlicks_PartialGeometricAttenuation(n, v, k) * Schlicks_PartialGeometricAttenuation(n, l, k);
}

#if 0

float G_Smith(float Roughness, float NoV, float NoL)
{
    float a2 = Roughness * Roughness;
    float G_V = NoV + sqrt((NoV - NoV * a2) * NoV + a2);
    float G_L = NoL + sqrt((NoL - NoL * a2) * NoL + a2);
    return rcp(G_V * G_L);
}
#endif

// http://graphicrants.blogspot.com.au/2013/08/specular-brdf-reference.html
float GGX(float NdotV, float a)
{
    float k = a / 2;
    return NdotV / (NdotV * (1.0f - k) + k);
}

// http://graphicrants.blogspot.com.au/2013/08/specular-brdf-reference.html
float G_Smith(float a, float nDotV, float nDotL)
{
    return GGX(nDotL, a * a) * GGX(nDotV, a * a);
}

float2 IntegrateBRDF(float Roughness, float NoV)
{
    float3 N = float3(0.0f, 0.0f, 1.0f);
    float3 V;
    V.x = sqrt(1.0f - NoV * NoV); // sin
    V.y = 0;
    V.z = NoV; // cos
    float A = 0;
    float B = 0;
    const uint NumSamples = 256;
    for (uint i = 0; i < NumSamples; i++)
    {
        float2 Xi = Hammersley(i, NumSamples);
        float3 H = ImportanceSampleGGX(Xi, Roughness, N);
        float3 L = 2 * dot(V, H) * H - V;
        float NoL = saturate(L.z);
        float NoH = saturate(H.z);
        float VoH = saturate(dot(V, H));
        if (NoL > 0)
        {
            float G = G_Smith(Roughness, NoV, NoL);
            float G_Vis = G * VoH / (NoH * NoV);
            float Fc = pow(1 - VoH, 5);
            A += (1 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    return float2(A, B) / NumSamples;
}

float4 main(PRECALC_BRDF_VS_OUTPUT input) : SV_TARGET
{
    float nDotV = input.Tex.x;
    float roughness = input.Tex.y;

    return float4(IntegrateBRDF(1.0f - roughness, nDotV), 0.0, 1.0f);
}