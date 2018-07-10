#pragma once
#ifndef HLSL
typedef UINT uint;
#endif

struct AverageSamplesConstants
{
	uint SampleCount;
	uint Width;
	uint Height;
};

#define AverageSampleThreadGroupWidth 8
#define AverageSampleThreadGroupHeight 8

#define AverageSamplesConstantRegister 0

#define AverageSamplesAccumulatedSamplesRegister 0
#define AverageSamplesOutputBuffer 1
