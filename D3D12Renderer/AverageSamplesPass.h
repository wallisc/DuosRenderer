#pragma once

class AverageSamplesPass
{
public:
	AverageSamplesPass(ID3D12Device &Device, UINT NodeMask);

	void AverageSamplesAndGammaCorrect(
		ID3D12GraphicsCommandList *pCommandList,
		UINT SampleCount,
		UINT Width,
		UINT Height,
		D3D12_GPU_DESCRIPTOR_HANDLE AccumulatedSampleBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE OutputBuffer);

private:
	enum {
		AverageSamplesAccumulatedSampleUAVSlot = 0,
		AverageSamplesOutputUAVSlot,
		AverageSamplesConstantSlot,
		AverageSamplesRootSignatureSize
	};

	CComPtr<ID3D12RootSignature> m_pRootSignature;
	CComPtr<ID3D12PipelineState> m_pPSO;
};