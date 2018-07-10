#define HLSL
#include "ShaderUtil.h"
#include "AverageSamplesShared.h"

ConstantBuffer<AverageSamplesConstants> Constants : CONSTANT_REGISTER(AverageSamplesConstantRegister);
RWTexture2D<float4> AccumulatedBuffer : UAV_REGISTER(AverageSamplesAccumulatedSamplesRegister);
RWTexture2D<float4> OutputBuffer : UAV_REGISTER(AverageSamplesOutputBuffer);

float4 GammaCorrect(float4 color)
{
	return pow(color, 1.0 / 2.2);
}

[numthreads(AverageSampleThreadGroupWidth, AverageSampleThreadGroupHeight, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	if (DTid.x >= Constants.Width || DTid.y >= Constants.Height)
	{
		return;
	}

	OutputBuffer[DTid.xy] = GammaCorrect(AccumulatedBuffer[DTid.xy] / Constants.SampleCount);
}
