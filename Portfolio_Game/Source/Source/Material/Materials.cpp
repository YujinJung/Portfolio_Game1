#include "UploadBuffer.h"
#include "Materials.h"

Materials::Materials()
	: mInBeginEndPair(false)
{
}

Materials::~Materials()
{
}

UINT Materials::GetSize() const
{
	return (UINT)mMaterials.size();
}

Material * Materials::Get(std::string Name)
{
	return mMaterials[Name].get();
}

void Materials::SetMaterial(
	const std::string& Name,
	const int& DiffuseSrvHeapIndex,
	const DirectX::XMFLOAT4& DiffuseAlbedo,
	const DirectX::XMFLOAT3& FresnelR0,
	const float& Roughness, const int& MatIndex)
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
void Materials::SetMaterial(
	const std::vector<std::string>& Name,
	const std::vector<int>& DiffuseSrvHeapIndex,
	const std::vector<DirectX::XMFLOAT4>& DiffuseAlbedo,
	const std::vector<DirectX::XMFLOAT3>& FresnelR0,
	const std::vector<float>& Roughness, int MatIndex)
{
	for (int i = 0; i < Name.size(); ++i)
	{
		auto temp = std::make_unique<Material>();
		temp->Name = Name[i];
		temp->MatCBIndex = MatIndex++;
		temp->DiffuseSrvHeapIndex = DiffuseSrvHeapIndex[i];
		temp->DiffuseAlbedo = DiffuseAlbedo[i];
		temp->FresnelR0 = FresnelR0[i];
		temp->Roughness = Roughness[i];

		mMaterials[Name[i]] = std::move(temp);
	}
}

void Materials::Begin(ID3D12Device * device, ID3D12DescriptorHeap * cbvHeap)
{
	if (mInBeginEndPair)
		throw std::exception("Cannot nest Begin calls on a Material");

	mDevice = device;
	mCbvHeap = cbvHeap;
	mInBeginEndPair = true;
}

void Materials::End()
{
	if (!mInBeginEndPair)
		throw std::exception("Begin must be called before End");

	mDevice = nullptr;
	mCbvHeap = nullptr;
	mInBeginEndPair = false;
}

void Materials::BuildConstantBufferViews(const std::vector<std::unique_ptr<FrameResource>> &mFrameResources, int mMatCbvOffset)
{
	UINT materialCount = GetSize();
	UINT materialCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
	UINT mCbvSrvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

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

			mDevice->CreateConstantBufferView(&cbvDesc, handle);
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

