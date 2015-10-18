struct VS_INPUT
{
	float4 Pos : POSITION;
	float4 ViewDirection : NORMAL0;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float4 ViewDirection : NORMAL0;
};

float3 ConvertTextureCubeDir(float3 dir)
{
	float x = dir.x;
	float y = -dir.y;
	float z = dir.z;
	return float3(x, y, z);
}

VS_OUTPUT VS(VS_INPUT input)
{
	VS_OUTPUT output;
	output.Pos = input.Pos;
	output.ViewDirection = input.ViewDirection;
	return output;
}

TextureCube environmentMap : register(t0);
SamplerState environmentSampler : register(s0);

float4 PS(VS_OUTPUT input) : SV_Target
{
	return environmentMap.Sample(environmentSampler, ConvertTextureCubeDir(input.ViewDirection.xyz));
}