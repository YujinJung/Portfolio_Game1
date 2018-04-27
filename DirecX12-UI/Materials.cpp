#include "Materials.h"
#include "../Common/UploadBuffer.h"


Materials::Materials()
{
}


Materials::~Materials()
{
}

void Materials::SetMaterial(std::string Name, int MatIndex, int DiffuseSrvHeapIndex, DirectX::XMFLOAT4 DiffuseAlbedo, DirectX::XMFLOAT3 FresnelR0, float Roughness)
{
	auto temp = std::make_unique<Material>();
	temp->Name = Name;
	temp->MatCBIndex = MatIndex;
	temp->DiffuseSrvHeapIndex = DiffuseSrvHeapIndex;
	temp->DiffuseAlbedo = DiffuseAlbedo;
	temp->FresnelR0 = FresnelR0;
	temp->Roughness = Roughness;

	mMaterials[Name] = std::move(temp);
}

Material * Materials::Get(std::string Name)
{
	return mMaterials[Name].get();
}

UINT Materials::GetSize() const
{
	return (UINT)mMaterials.size();
}

void Materials::BuildConstantBufferViews(ID3D12Device* device, ID3D12DescriptorHeap* mCbvHeap, const std::vector<std::unique_ptr<FrameResource>> &mFrameResources, int gNumFrameResources, int mMatCbvOffset)
{
	UINT materialCount = GetSize();
	UINT materialCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
	UINT mCbvSrvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto materialCB = mFrameResources[frameIndex]->MaterialCB->Resource();
		for (UINT i = 0; i < materialCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = materialCB->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * materialCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = mMatCbvOffset + frameIndex * materialCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = materialCBByteSize;

			device->CreateConstantBufferView(&cbvDesc, handle);
		}
	}
}

void Materials::UpdateMaterialCB(UploadBuffer<MaterialConstants>* currMaterialCB)
{
	for (auto& e : mMaterials)
	{
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			DirectX::XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			mat->NumFramesDirty--;
		}
	}
}

