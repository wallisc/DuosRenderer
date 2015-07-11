//--------------------------------------------------------------------------------------
// File: Tutorial07.fx
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
Texture2D txDiffuse : register( t0 );

SamplerState samLinear : register( s0 );

cbuffer cbCamera : register( b0 )
{
    float4x4 View;
	float4x4 Projection;
    float4 CamPos;
	float4 Dimensions;
	float FarClip;
};

cbuffer cbDirectionalLight : register(b1)
{
	float4 LightDirection;
	float4 LightColor;
};

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
    float4 CamPos : NORMAL1;
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

    output.CamPos = mul( View, input.Pos );
	output.Pos = mul(Projection, output.CamPos);
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
	float nDotL = dot(n, -LightDirection);

	output.Color = nDotL * LightColor * txDiffuse.Sample(samLinear, input.Tex);
	return output;
}
