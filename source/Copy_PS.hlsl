struct VertexOutput
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

float4 main(VertexOutput input) : SV_TARGET
{
	return float4(input.tex, 0, 1);
}