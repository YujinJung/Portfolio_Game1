#include "DXUI.h"
#include "Materials.h"

using namespace DirectX;
DXUI::DXUI()
{
	mWorldTransform.Scale = { 1.0f, 1.0f, 1.0f };
	mWorldTransform.Position = { 0.0f, 0.0f, 0.0f };
	UIoffset = { 0.0f, 0.0f, 0.0f };
}


DXUI::~DXUI()
{
}

UINT DXUI::GetSize() const
{
	return (UINT)mAllRitems.size();
}
const std::vector<RenderItem*> DXUI::GetRenderItem(eUIList Type)
{
	return mRitems[(int)Type];
}

void DXUI::SetPosition(FXMVECTOR inPosition)
{
	XMStoreFloat3(&mWorldTransform.Position, inPosition);
}
void DXUI::SetDamageScale(DirectX::FXMVECTOR inEyeLeft, float inScale)
{
	XMStoreFloat3(&UIoffset, (1.0f - inScale) * inEyeLeft * 0.1f);
	mWorldTransform.Scale.x = inScale;
}

void DXUI::BuildConstantBufferViews(ID3D12Device * device, ID3D12DescriptorHeap * mCbvHeap, const std::vector<std::unique_ptr<FrameResource>>& mFrameResources, int mUICbvOffset)
{
	UINT UICount = GetSize();
	UINT UICBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(UIConstants));
	UINT mCbvSrvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto UICB = mFrameResources[frameIndex]->UICB->Resource();

		for (UINT i = 0; i < UICount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = UICB->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * UICBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = mUICbvOffset + frameIndex * UICount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = UICBByteSize;

			device->CreateConstantBufferView(&cbvDesc, handle);
		}
	}
}

void DXUI::BuildRenderItem(std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>& mGeometries, Materials & mMaterials)
{
	int UIIndex = 0;
	// TODO : Setting the Name

	// place over the head
	auto temp = std::make_unique<RenderItem>();
	// atan(theta) : Theta is associated with PlayerCamera
	XMStoreFloat4x4(&temp->World, XMMatrixScaling(0.01f, 0.01f, 0.001f) * XMMatrixRotationX(atan(3.0f / 2.0f)) * XMMatrixRotationY(XM_PI) * XMMatrixTranslation(0.0f, 0.87f, 0.0f));
	temp->TexTransform = MathHelper::Identity4x4();
	temp->Mat = mMaterials.Get("ice0");
	temp->Geo = mGeometries["shapeGeo"].get();
	temp->ObjCBIndex = UIIndex++;
	temp->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	temp->StartIndexLocation = temp->Geo->DrawArgs["grid"].StartIndexLocation;
	temp->BaseVertexLocation = temp->Geo->DrawArgs["grid"].BaseVertexLocation;
	temp->IndexCount = temp->Geo->DrawArgs["grid"].IndexCount;
	mRitems[(int)eUIList::Rect].push_back(temp.get());
	mAllRitems.push_back(std::move(temp));

	auto mana = std::make_unique<RenderItem>();
	// atan(theta) : Theta is associated with PlayerCamera
	XMStoreFloat4x4(&mana->World, XMMatrixScaling(0.01f, 0.01f, 0.001f) * XMMatrixRotationX(atan(3.0f / 2.0f)) * XMMatrixRotationY(XM_PI) * XMMatrixTranslation(0.0f, 0.89f, 0.0f));
	mana->TexTransform = MathHelper::Identity4x4();
	mana->Mat = mMaterials.Get("red");
	mana->Geo = mGeometries["shapeGeo"].get();
	mana->ObjCBIndex = UIIndex++;
	mana->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	mana->StartIndexLocation = mana->Geo->DrawArgs["grid"].StartIndexLocation;
	mana->BaseVertexLocation = mana->Geo->DrawArgs["grid"].BaseVertexLocation;
	mana->IndexCount = mana->Geo->DrawArgs["grid"].IndexCount;
	mRitems[(int)eUIList::Rect].push_back(mana.get());
	mAllRitems.push_back(std::move(mana));
}

void DXUI::UpdateUICBs(UploadBuffer<UIConstants>* currUICB, XMMATRIX playerWorld, bool mTransformDirty)
{
	for (auto& e : mAllRitems)
	{
		// if Transform then Reset the Dirty flag
		if (mTransformDirty) { e->NumFramesDirty = gNumFrameResources; }
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX T = XMMatrixTranslation(mWorldTransform.Position.x + UIoffset.x, mWorldTransform.Position.y + UIoffset.y, mWorldTransform.Position.z + UIoffset.z);
			XMMATRIX world = XMLoadFloat4x4(&e->World) * playerWorld * T;
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			UIConstants uiConstants;
			XMMATRIX S = XMMatrixScaling(mWorldTransform.Scale.x, mWorldTransform.Scale.y, mWorldTransform.Scale.z);

			XMStoreFloat4x4(&uiConstants.World, XMMatrixTranspose(world) * S);
			XMStoreFloat4x4(&uiConstants.TexTransform, XMMatrixTranspose(texTransform));

			currUICB->CopyData(e->ObjCBIndex, uiConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

