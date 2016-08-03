#include "PrefilterCube.h"

PREFILTERED_CUBE_VS_OUTPUT main(PREFILTERED_CUBE_VS_INPUT input)
{
    PREFILTERED_CUBE_VS_OUTPUT output;
    output.Pos = input.Pos;
    output.Norm = input.Norm;
    return output;
}
