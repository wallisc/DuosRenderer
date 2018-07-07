#pragma once
#include "D3D12RendererInternal.h"

class D3D12Camera : public virtual Camera
{
public:
	virtual ~D3D12Camera() {}
	D3D12Camera(CreateCameraDescriptor &desc);

	virtual void Translate(_In_ const Vec3 &translationVector) { assert(false); }
	virtual void Rotate(float row, float yaw, float pitch) { assert(false); }

	const SceneConstantBuffer &GetConstantData()
	{
		return m_Constants;
	}

private:
	SceneConstantBuffer m_Constants;
	CreateCameraDescriptor m_Desc;
};