#include "PrecalcBRDF.h"

PRECALC_BRDF_VS_OUTPUT main(PRECALC_BRDF_VS_INPUT input)
{
    PRECALC_BRDF_VS_OUTPUT output;
    output.Pos = input.Pos;
    output.Norm = input.Norm;
    return output;
}
