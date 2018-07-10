#include "stdafx.h"
#include <DirectXMath.h>
#include <time.h>

using namespace DirectX;

D3D12Camera::D3D12Camera(CreateCameraDescriptor &desc) : m_Desc(desc) 
{

	XMVECTOR Eye = { m_Desc.m_FocalPoint.x, m_Desc.m_FocalPoint.y, m_Desc.m_FocalPoint.z, 1.0f };
	XMVECTOR LookAt = { m_Desc.m_LookAt.x, m_Desc.m_LookAt.y, m_Desc.m_LookAt.z, 1.0f };
	XMVECTOR Up = { m_Desc.m_Up.x, m_Desc.m_Up.y, m_Desc.m_Up.z, 0.0f };

	float fovAngleY = desc.m_VerticalFieldOfView;
	float aspectRatio = (float)m_Desc.m_Width / (float)m_Desc.m_Height;
	XMMATRIX view = XMMatrixLookAtLH(Eye, LookAt, Up);
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), aspectRatio, 1.0f, 125.0f);
	XMMATRIX viewProj = view * proj;

	auto invProjViewMatrix = XMMatrixInverse(nullptr, viewProj);

	m_Constants.cameraPosition = { desc.m_FocalPoint.x, desc.m_FocalPoint.y, desc.m_FocalPoint.z, 1.0 };
	XMStoreFloat4x4((XMFLOAT4X4*)&m_Constants.projectionToWorld.v0, invProjViewMatrix);
}

const SceneConstantBuffer &D3D12Camera::GetConstantData()
{
	static uint count = 0;

	m_Constants.time = count += 100;
	return m_Constants;
}
