#define EPSILON 0.0001

Texture2D DiffuseTexture : register(t0);
Texture2D shadowBuffer : register(t1);

SamplerState samLinear : register( s0 );

cbuffer cbCamera : register( b0 )
{
    float4 CamPos;
	float4 Dimensions;
	float FarClip;
};

cbuffer cbViewProjectionTransforms : register(b1)
{
	float4x4 View;
	float4x4 Projection;
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

//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float4 Pos : POSITION;
    float4 Norm : NORMAL0;
	float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
    float4 Norm : NORMAL0;
	float4 LightPos : NORMAL1;
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

	output.Pos = mul(Projection, mul(View, input.Pos));
	output.LightPos = mul(LightProjection, mul(LightView, input.Pos));
    output.Norm = input.Norm;
    output.Tex = input.Tex;
    
    return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
PS_OUTPUT PS( PS_INPUT input) : SV_Target
{
    PS_OUTPUT output;

	// Calculating in pixel shader for precision
	float3 n = normalize(input.Norm.xyz);
	float nDotL = dot(n, LightDirection);

	//input.LightPos.xy /= float2(800, 600);
	input.LightPos /= input.LightPos.w;

	input.LightPos.xy = (input.LightPos.xy + float2(1.0f, 1.0f))/ float2(2.0f, -2.0f);

	output.Color = float4(0.0f, 0.0f, 0.0f, 1.0f);
	float ShadowMapDepth = shadowBuffer.Sample(samLinear, input.LightPos.xy).r;
	if (input.LightPos.z <= ShadowMapDepth + EPSILON)
	{
		output.Color += (nDotL > 0.0f) * (nDotL * LightColor * DiffuseTexture.Sample(samLinear, input.Tex));
	}
	return output;
}
