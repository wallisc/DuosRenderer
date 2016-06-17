#define EPSILON 0.0001
#define PI 3.14
#define ENABLE_PBR 1
#define BUMP_FACTOR 0.5


// sqrt(2.0 / PI)
#define SQRT_2_DIVIDED_BY_PI 0.7978845608

Texture2D DiffuseTexture : register(t0);
Texture2D shadowBuffer : register(t1);
TextureCube EnvironmentMap : register(t2);
TextureCube IrradianceMap : register(t3);
Texture2D NormalMap : register(t4);

SamplerState samLinear : register( s0 );

cbuffer cbCamera : register( b0 )
{
    float4 Dimensions;
    float FarClip;
};

cbuffer cbViewProjectionTransforms : register(b1)
{
    float4x4 View;
    float4x4 Projection;
    float4x4 InvTransView;
    float4 CamPos;
}

cbuffer cbDirectionalLight : register(b2)
{
    float4 LightDirection;
    float4 LightColor;
};

cbuffer cbViewProjectionTransforms : register(b3)
{
    float4x4 LightView;
    float4x4 LightProjection;
}

cbuffer cbMaterial : register(b4)
{
    float4 DiffuseColor;
    float4 MaterialProperties; // (Reflectivity (R0), Roughness (a), ,)
}

//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float4 Pos : POSITION;
    float4 Norm : NORMAL0;
    float2 Tex : TEXCOORD0;
    float4 Tangent : NORMAL1;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 ViewPos : POSITION0;
    float4 WorldPos : POSITION2;
    float2 Tex : TEXCOORD0;
    float4 Norm : NORMAL0;
    float4 Tangent : NORMAL1;
    float4 Binorm : NORMAL2;
    float4 LightPos : NORMAL3;
    float4 WorldNorm : NORMAL4;
};

struct PS_OUTPUT
{
    float4 Color : COLOR0;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS( VS_INPUT input )
{
    PS_INPUT output;

    output.ViewPos = mul(View, input.Pos);
    output.Pos = mul(Projection, output.ViewPos);
    output.LightPos = mul(LightProjection, mul(LightView, input.Pos));
    output.Norm = mul(InvTransView, float4(input.Norm.xyz, 0.0));
    output.Tex = input.Tex;
    output.WorldPos = input.Pos;
    output.WorldNorm = input.Norm;
    output.Tangent = mul(InvTransView, float4(input.Tangent.xyz, 0.0));
    output.Binorm = float4(cross(output.Tangent, output.Norm), 0.0);

    return output;
}

float GGX_Distribution(float3 h, float3 n, float roughness)
{
    float nDotH = saturate(dot(n, h));
    float nDotHSquared = nDotH * nDotH;
    float roughnessSquared = roughness * roughness;
    float Quotient = nDotHSquared * (roughnessSquared - 1) + 1;
    return roughnessSquared / (PI * Quotient * Quotient);
}

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

float Fresnel(float3 h, float3 n, float reflectivity)
{
    return (reflectivity + (1.0 - reflectivity) * pow(1 - saturate(dot(h, n)), 5));
}

float BRDF(float3 v, float3 n, float3 l, float roughness, float baseReflectivity, out float fresnel)
{
    float3 h = normalize(l + v);
    float nDotL = saturate(dot(n, l));
    float nDotV = saturate(dot(n, v));

    float G = Schlicks_GeometricAttenuation(v, l, n, roughness);
    fresnel = Fresnel(h, n, baseReflectivity);
    float D = GGX_Distribution(h, n, roughness);
    return fresnel * D * G / (4 * nDotL * nDotV);
}

#if USE_NORMAL_MAP
float3 GetNormal(PS_INPUT input)
{
    float3 norm = normalize(input.Norm.xyz);
    float3 tangent = normalize(input.Tangent.xyz);
    float3 binorm = normalize(input.Binorm.xyz);
    float2 NormalMapVec = BUMP_FACTOR * (2.0 * NormalMap.Sample(samLinear, input.Tex.xy).xy - 1.0);
    float z = sqrt(1.0 - dot(NormalMapVec, NormalMapVec));
    return normalize(NormalMapVec.x * tangent + NormalMapVec.y * binorm + z * norm);
}
#else
float3 GetNormal(PS_INPUT input)
{
    return normalize(input.Norm.xyz);
}
#endif


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
PS_OUTPUT PS( PS_INPUT input) : SV_Target
{
    PS_OUTPUT output;

    // Calculating in pixel shader for precision
    float3 n = GetNormal(input);
    float3 v = normalize(-input.ViewPos.xyz);
    float R0 = MaterialProperties.r;
    float roughness = MaterialProperties.g;

    float nDotL = saturate(dot(n, LightDirection));
    float nDotV = saturate(dot(n, v));

    input.LightPos /= input.LightPos.w;

    input.LightPos.xy = (input.LightPos.xy + float2(1.0f, 1.0f))/ float2(2.0f, -2.0f);

    float3 totalDiffuse = float3(0.0f, 0.0f, 0.0f);
    float3 totalSpecular = float3(0.0f, 0.0f, 0.0f);
    float totalFresnel = 0.0;

    float ShadowMapDepth = shadowBuffer.Sample(samLinear, input.LightPos.xy).r;
    if (input.LightPos.z <= ShadowMapDepth + EPSILON && nDotL > 0.0)
    {
#if USE_TEXTURE
        float4 diffuse = DiffuseTexture.Sample(samLinear, input.Tex);
#else
        float4 diffuse = DiffuseColor;
#endif
        totalDiffuse += (nDotL > 0.0f) * (nDotL * LightColor * diffuse);

#if ENABLE_PBR
        float reflectivity;
        totalSpecular += LightColor * BRDF(v, n, LightDirection, roughness, R0, reflectivity);
        totalFresnel += reflectivity;
#endif
    }
#if ENABLE_PBR
    if (1)
    {
        float3 reflectionRay = reflect(-v, n);
        float3 incomingViewDir = normalize(CamPos.xyz - input.WorldPos.xyz);
        // TODO: This won't work for bump maps
        float3 worldNorm = normalize(input.WorldNorm.xyz);
        float3 worldReflectionRay = reflect(-incomingViewDir, worldNorm);

        float3 reflectionColor = EnvironmentMap.Sample(samLinear, worldReflectionRay);
        float reflectivity;
        totalSpecular += reflectionColor * BRDF(v, n, reflectionRay, roughness, R0, reflectivity);
        totalFresnel += reflectivity;
    }

    uint SampleCount = 2;
    totalSpecular /= SampleCount;
    totalFresnel /= SampleCount;
    output.Color = saturate(float4(totalDiffuse * (1.0 - totalFresnel) + totalSpecular, 1.0));
#else
    output.Color = float4(totalDiffuse, 1.0);
#endif

    return output;
}
