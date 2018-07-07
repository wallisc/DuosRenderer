#pragma once
#include <memory>

void ThrowD3D12Failure();
static void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		ThrowD3D12Failure();
	}
}

template<class T, class U>
std::shared_ptr<T> safe_dynamic_cast(const std::shared_ptr<U> &sp) {
	auto spCast = std::dynamic_pointer_cast<T>(sp);
	if (!spCast)
	{
		ThrowD3D12Failure();
	}
	return spCast;
}