#include "PostProcess.h"

Texture2D InputTexture : register(t0);
SamplerState Sampler : register(s0);

float4 PS(POST_PROCESS_VS_OUTPUT input) : SV_Target
{
	return InputTexture.Sample(Sampler, input.UV);
}