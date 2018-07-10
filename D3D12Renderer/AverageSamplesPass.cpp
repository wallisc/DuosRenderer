#include "stdafx.h"
#include "AverageSamplesShared.h"
#include "CompiledShaders\AverageSamples.h"

AverageSamplesPass::AverageSamplesPass(ID3D12Device &Device, UINT NodeMask)
{
	CD3DX12_ROOT_PARAMETER1 parameters[AverageSamplesRootSignatureSize];
	
	CD3DX12_DESCRIPTOR_RANGE1 accumulatedDescriptorRange(
		D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
		1,
		AverageSamplesAccumulatedSamplesRegister);
	parameters[AverageSamplesAccumulatedSampleUAVSlot].InitAsDescriptorTable(1, &accumulatedDescriptorRange);

	CD3DX12_DESCRIPTOR_RANGE1 outputUAVDescriptorRange(
		D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
		1,
		AverageSamplesOutputBuffer);
	parameters[AverageSamplesOutputUAVSlot].InitAsDescriptorTable(1, &outputUAVDescriptorRange);
	parameters[AverageSamplesConstantSlot].InitAsConstants(SizeOfInUint32(AverageSamplesConstants), AverageSamplesConstantRegister);
	
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(parameters), parameters);
	CreateRootSignature(Device, desc, NodeMask, &m_pRootSignature);

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.CS = CD3DX12_SHADER_BYTECODE(g_pAverageSamples, sizeof(g_pAverageSamples));
	psoDesc.NodeMask = NodeMask;
	psoDesc.pRootSignature = m_pRootSignature;
	Device.CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_pPSO));
}

void AverageSamplesPass::AverageSamplesAndGammaCorrect(
	ID3D12GraphicsCommandList *pCommandList,
	UINT SampleCount,
	UINT Width,
	UINT Height,
	D3D12_GPU_DESCRIPTOR_HANDLE AccumulatedSampleBuffer,
	D3D12_GPU_DESCRIPTOR_HANDLE OutputBuffer)
{
	pCommandList->SetComputeRootSignature(m_pRootSignature);
	pCommandList->SetPipelineState(m_pPSO);
	pCommandList->SetComputeRootDescriptorTable(AverageSamplesAccumulatedSampleUAVSlot, AccumulatedSampleBuffer);
	pCommandList->SetComputeRootDescriptorTable(AverageSamplesOutputUAVSlot, OutputBuffer);

	AverageSamplesConstants constants;
	constants.SampleCount = SampleCount;
	constants.Width = Width;
	constants.Height = Height;
	pCommandList->SetComputeRoot32BitConstants(AverageSamplesConstantSlot, SizeOfInUint32(AverageSamplesConstants), &constants, 0);

	UINT dispatchWidth = DivideAndRoundUp<UINT>(Width, AverageSampleThreadGroupWidth);
	UINT dispatchHeight = DivideAndRoundUp<UINT>(Height, AverageSampleThreadGroupHeight);
	pCommandList->Dispatch(dispatchWidth, dispatchHeight, 1);

}