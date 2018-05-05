#include "MonsterUI.h"
#include "Materials.h"

using namespace DirectX;

MonsterUI::MonsterUI()
{
	mWorldTransform.Scale = { 10.0f, 10.0f, 10.0f };
	mWorldTransform.Position = { 0.0f, 0.0f, 0.0f };
	UIoffset = { 0.0f, 10.0f, 0.0f };
}


MonsterUI::~MonsterUI()
{
}

UINT MonsterUI::GetSize() const
{
	return (UINT)mAllRitems.size();
}

const std::vector<RenderItem*> MonsterUI::GetRenderItem(eUIList Type)const
{
	return mRitems[(int)Type];
}

void MonsterUI::BuildRenderItem(
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>& mGeometries,
	Materials & mMaterials,
	UINT numOfMonster)
{
	int UIIndex = 0;
	for (UINT i = 0; i < numOfMonster; ++i)
	{
		// place over the head
		auto frontHealthBar = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&frontHealthBar->World, XMMatrixScaling(0.04f, 1.0f, 0.004f)  * XMMatrixRotationX(XM_PIDIV2) * XMMatrixTranslation(0.0f, -0.5f, 0.0f));
		frontHealthBar->TexTransform = MathHelper::Identity4x4();
		frontHealthBar->Mat = mMaterials.Get("red");
		frontHealthBar->Geo = mGeometries["shapeGeo"].get();
		frontHealthBar->ObjCBIndex = UIIndex++;
		frontHealthBar->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		frontHealthBar->StartIndexLocation = frontHealthBar->Geo->DrawArgs["grid"].StartIndexLocation;
		frontHealthBar->BaseVertexLocation = frontHealthBar->Geo->DrawArgs["grid"].BaseVertexLocation;
		frontHealthBar->IndexCount = frontHealthBar->Geo->DrawArgs["grid"].IndexCount;

		auto backHealthBar = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&backHealthBar->World, XMMatrixScaling(0.04f, 1.0f, 0.003f)  * XMMatrixRotationX(-XM_PIDIV2) * XMMatrixTranslation(0.0f, -0.5f, 0.0f));
		backHealthBar->TexTransform = MathHelper::Identity4x4();
		backHealthBar->Mat = mMaterials.Get("red");
		backHealthBar->Geo = mGeometries["shapeGeo"].get();
		backHealthBar->ObjCBIndex = UIIndex++;
		backHealthBar->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		backHealthBar->StartIndexLocation = backHealthBar->Geo->DrawArgs["grid"].StartIndexLocation;
		backHealthBar->BaseVertexLocation = backHealthBar->Geo->DrawArgs["grid"].BaseVertexLocation;
		backHealthBar->IndexCount = backHealthBar->Geo->DrawArgs["grid"].IndexCount;

		mRitems[(int)eUIList::Rect].push_back(frontHealthBar.get());
		mAllRitems.push_back(std::move(frontHealthBar));
		mRitems[(int)eUIList::Rect].push_back(backHealthBar.get());
		mAllRitems.push_back(std::move(backHealthBar)); 	
	}
}
void MonsterUI::BuildConstantBufferViews(
	ID3D12Device * device,
	ID3D12DescriptorHeap * mCbvHeap,
	const std::vector<std::unique_ptr<FrameResource>>& mFrameResources,
	int mMonsterUICbvOffset)
{
	UINT UICount = (UINT)mAllRitems.size();
	UINT UICBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(UIConstants));
	UINT mCbvSrvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto UICB = mFrameResources[frameIndex]->MonsterUICB->Resource();

		for (UINT i = 0; i < UICount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = UICB->GetGPUVirtualAddress();

			cbAddress += i * UICBByteSize;
			int heapIndex = mMonsterUICbvOffset + frameIndex * UICount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = UICBByteSize;

			device->CreateConstantBufferView(&cbvDesc, handle);
		}
	}
}

void MonsterUI::UpdateUICBs(
	UploadBuffer<UIConstants>* currUICB,
	std::vector<XMMATRIX> playerWorlds,
	bool mTransformDirty)
{
	int uIndex = 0;
	int mIndex = -1;
	int mOffset = GetSize() / playerWorlds.size();
	// need to check c Indexcheck
	for (auto& e : mAllRitems)
	{
		if (mTransformDirty)
		{
			e->NumFramesDirty = gNumFrameResources;
		}
		if (mIndex != uIndex / mOffset)
		{
			mIndex = uIndex / mOffset;
		}

		if (e->NumFramesDirty > 0)
		{
			XMMATRIX T = XMMatrixTranslation(
				mWorldTransform.Position.x + UIoffset.x,
				mWorldTransform.Position.y + UIoffset.y,
				mWorldTransform.Position.z + UIoffset.z);
			XMMATRIX world = XMLoadFloat4x4(&e->World) * playerWorlds[mIndex] * T;
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);
			XMMATRIX S = XMMatrixScaling(
				mWorldTransform.Scale.x,
				mWorldTransform.Scale.y,
				mWorldTransform.Scale.z);

			UIConstants uiConstants;
			XMStoreFloat4x4(&uiConstants.World, XMMatrixTranspose(world) );
			XMStoreFloat4x4(&uiConstants.TexTransform, XMMatrixTranspose(texTransform));

			currUICB->CopyData(e->ObjCBIndex, uiConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
		
		uIndex++;
	}
}