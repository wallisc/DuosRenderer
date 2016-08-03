struct PREFILTERED_CUBE_VS_INPUT
{
    float4 Pos : POSITION;
    float4 Norm : NORMAL0;
};

struct PREFILTERED_CUBE_VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float3 Norm : NORMAL0;
};