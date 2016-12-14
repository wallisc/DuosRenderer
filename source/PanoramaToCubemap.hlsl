#include "ShaderUtil.h"

struct PREFILTERED_CUBE_VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float3 Norm : NORMAL0;
};

Texture2D InputTexture : register(t0);
SamplerState Sampler : register(s0);
float4 main(PREFILTERED_CUBE_VS_OUTPUT input) : SV_TARGET
{

    float3 viewDir = normalize(input.Norm.xyz);
    float2 uv;

    {
        float p = atan2(viewDir.z, viewDir.x);
        p = p > 0 ? p : p + 2 * 3.14;
        uv.x = p / (2 * 3.14);
    }
    
    uv.y = acos(viewDir.y) / (3.14);
    return InputTexture.Sample(Sampler, uv);
}