#include "PostProcess.h"

POST_PROCESS_VS_OUTPUT VS(uint id : SV_VertexID)
{
   POST_PROCESS_VS_OUTPUT output;
   output.UV = float2((id << 1) & 2, id & 2);
   output.Pos = float4(output.UV * float2(2, -2) + float2(-1, 1), 0, 1);
   return output;
}
