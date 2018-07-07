#pragma once
#include "D3D12RendererInternal.h"

class D3D12Light : public Light
{
public:
	virtual ~D3D12Light() {}
	D3D12Light(CreateLightDescriptor &desc)
	{
		assert(false);
	}
};
