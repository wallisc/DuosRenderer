#include "FullscreenPlane.h"

Texture2D InputTexture : register(t0);
SamplerState Sampler : register(s0);

float4 PS(FULLSCREEN_PLANE_VS_OUTPUT input) : SV_Target
{
    return InputTexture.Sample(Sampler, input.UV);
}