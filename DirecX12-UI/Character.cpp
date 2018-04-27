#include <string>
#include "../Common/MathHelper.h"
#include "../Common/GameTimer.h"
#include "FrameResource.h"
#include "Character.h"
#include <DirectXMath.h>

using namespace DirectX;
using namespace DirectX::PackedVector;

Character::Character()
{
}


Character::~Character()
{
}

UINT Character::GetCharacterMeshSize() const
{
	return (UINT)mRitems[(int)RenderLayer::Character].size();
}

UINT Character::GetAllRitemsSize() const
{
	return (UINT)mAllRitems.size();
}

UINT Character::GetBoneSize() const
{
	return (UINT)mSkinnedInfo.BoneCount();
}

const std::vector<RenderItem*> Character::GetRenderItem(RenderLayer type)
{
	return mRitems[(int)type];
}

void Character::BuildConstantBufferViews(ID3D12Device * device, ID3D12DescriptorHeap * mCbvHeap, const std::vector<std::unique_ptr<FrameResource>>& mFrameResources, int gNumFrameResources, int mChaCbvOffset)
{
	UINT characterCount = GetAllRitemsSize();
	UINT chaCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(SkinnedConstants));
	UINT mCbvSrvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto characterCB = mFrameResources[frameIndex]->SkinnedCB->Resource();

		for (UINT i = 0; i < characterCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = characterCB->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * chaCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = mChaCbvOffset + frameIndex * characterCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = chaCBByteSize;

			device->CreateConstantBufferView(&cbvDesc, handle);
		}
	}
}

void Character::BuildGeometry(ID3D12Device * device, ID3D12GraphicsCommandList* cmdList, const std::vector<SkinnedVertex>& inVertices, const std::vector<std::uint16_t>& inIndices, const SkinnedData& inSkinInfo)
{
	mSkinnedInfo = inSkinInfo;

	mSkinnedModelInst = std::make_unique<SkinnedModelInstance>();
	mSkinnedModelInst->SkinnedInfo = &mSkinnedInfo;
	mSkinnedModelInst->FinalTransforms.resize(mSkinnedInfo.BoneCount());
	mSkinnedModelInst->ClipName = mSkinnedInfo.GetAnimationName(0);
	mSkinnedModelInst->TimePos = 0.0f;

	if (inVertices.size() == 0)
	{
		MessageBox(0, L"Fbx not found", 0, 0);
		return;
	}

	UINT vCount = 0, iCount = 0;
	vCount = inVertices.size();
	iCount = inIndices.size();

	const UINT vbByteSize = (UINT)inVertices.size() * sizeof(SkinnedVertex);
	const UINT ibByteSize = (UINT)inIndices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "FbxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), inVertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), inIndices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, inVertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, inIndices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(SkinnedVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	auto vSubmeshOffset = mSkinnedInfo.GetSubmeshOffset();
	for (int i = 0; i < vSubmeshOffset.size(); ++i)
	{
		static UINT SubmeshOffsetIndex = 0;
		UINT CurrSubmeshOffsetIndex = vSubmeshOffset[i];

		SubmeshGeometry FbxSubmesh;
		FbxSubmesh.IndexCount = CurrSubmeshOffsetIndex;
		FbxSubmesh.StartIndexLocation = SubmeshOffsetIndex;
		FbxSubmesh.BaseVertexLocation = 0;

		std::string SubmeshName = "submesh_";
		SubmeshName.push_back(i + 48);
		geo->DrawArgs[SubmeshName] = FbxSubmesh;

		SubmeshOffsetIndex += CurrSubmeshOffsetIndex;
	}

	mGeometry = std::move(geo);
}

void Character::BuildRenderItem(Materials& mMaterials)
{
	int chaIndex = 0;
	int BoneCount = GetBoneSize();

	for (int i = 0; i < BoneCount - 1; ++i)
	{
		std::string SubmeshName = "submesh_";
		SubmeshName.push_back(i + 48); // ASCII
		std::string MaterialName = "material_0";
		// TODO : Setting the Name

		auto FbxRitem = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&FbxRitem->World, XMMatrixScaling(4.0f, 4.0f, 4.0f));
		FbxRitem->TexTransform = MathHelper::Identity4x4();
		FbxRitem->Mat = mMaterials.Get(MaterialName);
		FbxRitem->Geo = mGeometry.get();
		FbxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		FbxRitem->StartIndexLocation = FbxRitem->Geo->DrawArgs[SubmeshName].StartIndexLocation;
		FbxRitem->BaseVertexLocation = FbxRitem->Geo->DrawArgs[SubmeshName].BaseVertexLocation;
		FbxRitem->IndexCount = FbxRitem->Geo->DrawArgs[SubmeshName].IndexCount;

		FbxRitem->SkinnedCBIndex = chaIndex++;
		FbxRitem->SkinnedModelInst = mSkinnedModelInst.get();

		mRitems[(int)RenderLayer::Character].push_back(FbxRitem.get());
		mAllRitems.push_back(std::move(FbxRitem));
	}

	for (auto& e : mRitems[(int)RenderLayer::Character])
	{
		auto shadowedObjectRitem = std::make_unique<RenderItem>();
		*shadowedObjectRitem = *e;
		shadowedObjectRitem->Mat = mMaterials.Get("shadow0");
		shadowedObjectRitem->NumFramesDirty = gNumFrameResources;

		shadowedObjectRitem->SkinnedCBIndex = chaIndex++;
		shadowedObjectRitem->SkinnedModelInst = mSkinnedModelInst.get();

		mRitems[(int)RenderLayer::Shadow].push_back(shadowedObjectRitem.get());
		mAllRitems.push_back(std::move(shadowedObjectRitem));
	}
}

void Character::UpdateCharacterCBs(UploadBuffer<SkinnedConstants>* currSkinnedCB, const Light& mMainLight, const GameTimer & gt)
{
	UpdateCharacterShadows(mMainLight);
	// if(Animation) ...
	mSkinnedModelInst->UpdateSkinnedAnimation(gt.DeltaTime());

	for (auto& e : mAllRitems)
	{
		if (e->NumFramesDirty > 0)
		{
			SkinnedConstants skinnedConstants;

			std::copy(
				std::begin(mSkinnedModelInst->FinalTransforms),
				std::end(mSkinnedModelInst->FinalTransforms),
				&skinnedConstants.BoneTransforms[0]);

			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			XMStoreFloat4x4(&skinnedConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&skinnedConstants.TexTransform, XMMatrixTranspose(texTransform));

			currSkinnedCB->CopyData(e->SkinnedCBIndex, skinnedConstants);

			// Next FrameResource need to be updated too.

			// TODO:
			//e->NumFramesDirty--;
		}
	}
}

void Character::UpdateCharacterShadows(const Light& mMainLight)
{
	int i = 0;
	for (auto& e : mRitems[(int)RenderLayer::Shadow])
	{
		// Load the object world
		auto& o = mRitems[(int)RenderLayer::Character][i];
		XMMATRIX shadowWorld = XMLoadFloat4x4(&o->World);

		XMVECTOR shadowPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		XMVECTOR toMainLight = -XMLoadFloat3(&mMainLight.Direction);
		XMMATRIX S = XMMatrixShadow(shadowPlane, toMainLight);
		XMMATRIX shadowOffsetY = XMMatrixTranslation(0.0f, 0.001f, 0.0f);
		XMStoreFloat4x4(&e->World, shadowWorld * S * shadowOffsetY);
		e->NumFramesDirty = gNumFrameResources;

		//printMatrix(L"shadow ", i, XMLoadFloat4x4(&e->World));
		++i;
	}

}
