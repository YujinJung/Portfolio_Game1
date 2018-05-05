#include "DXUI.h"
#include "Materials.h"

using namespace DirectX;

DXUI::DXUI()
{
	mWorldTransform.Scale = { 1.0f, 1.0f, 1.0f };
	mWorldTransform.Position = { 0.0f, 0.0f, 0.0f };
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
void DXUI::SetDamageScale(float inScale)
{
	mWorldTransform.Scale.x = inScale;
}

void DXUI::BuildConstantBufferViews(
	ID3D12Device * device,
	ID3D12DescriptorHeap * mCbvHeap,
	const std::vector<std::unique_ptr<FrameResource>>& mFrameResources,
	int mUICbvOffset)
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
	auto frontHealthBar = std::make_unique<RenderItem>();
	// atan(theta) : Theta is associated with PlayerCamera
	XMStoreFloat4x4(&frontHealthBar->World, XMMatrixScaling(0.01f, 1.0f, 0.0011f) * XMMatrixRotationX(atan(3.0f / 2.0f)) * XMMatrixRotationY(XM_PI)  * XMMatrixTranslation(0.0f, 0.895f, 0.0f));
	frontHealthBar->TexTransform = MathHelper::Identity4x4();
	frontHealthBar->Mat = mMaterials.Get("ice0");
	frontHealthBar->Geo = mGeometries["shapeGeo"].get();
	frontHealthBar->ObjCBIndex = UIIndex++;
	frontHealthBar->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	frontHealthBar->StartIndexLocation = frontHealthBar->Geo->DrawArgs["grid"].StartIndexLocation;
	frontHealthBar->BaseVertexLocation = frontHealthBar->Geo->DrawArgs["grid"].BaseVertexLocation;
	frontHealthBar->IndexCount = frontHealthBar->Geo->DrawArgs["grid"].IndexCount;
	mRitems[(int)eUIList::Rect].push_back(frontHealthBar.get());
	mAllRitems.push_back(std::move(frontHealthBar));

	auto bgHealthBar = std::make_unique<RenderItem>();
	// atan(theta) : Theta is associated with PlayerCamera
	XMStoreFloat4x4(&bgHealthBar->World, XMMatrixScaling(0.01f, 1.0f, 0.001f) * XMMatrixRotationX(atan(3.0f / 2.0f)) * XMMatrixRotationY(XM_PI)  * XMMatrixTranslation(0.0f, 0.89f, 0.05f));
	bgHealthBar->TexTransform = MathHelper::Identity4x4();
	bgHealthBar->Mat = mMaterials.Get("bricks0");
	bgHealthBar->Geo = mGeometries["shapeGeo"].get();
	bgHealthBar->ObjCBIndex = UIIndex++;
	bgHealthBar->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	bgHealthBar->StartIndexLocation = bgHealthBar->Geo->DrawArgs["grid"].StartIndexLocation;
	bgHealthBar->BaseVertexLocation = bgHealthBar->Geo->DrawArgs["grid"].BaseVertexLocation;
	bgHealthBar->IndexCount = bgHealthBar->Geo->DrawArgs["grid"].IndexCount;
	mRitems[(int)eUIList::Rect].push_back(bgHealthBar.get());
	mAllRitems.push_back(std::move(bgHealthBar));
}

void DXUI::UpdateUICBs(
	UploadBuffer<UIConstants>* curUICB,
	XMMATRIX playerWorld,
	XMVECTOR inEyeLeft,
	bool mTransformDirty)
{
	for (auto& e : mAllRitems)
	{
		// if Transform then Reset the Dirty flag
		if (mTransformDirty) { e->NumFramesDirty = gNumFrameResources; }

		if (e->NumFramesDirty > 0)
		{
			// Health Bar move to left
			XMVECTOR UIoffset = (1.0f- mWorldTransform.Scale.x) * inEyeLeft * 0.1f;

			XMMATRIX T = XMMatrixTranslation(
				mWorldTransform.Position.x + UIoffset.m128_f32[0],
				mWorldTransform.Position.y + UIoffset.m128_f32[1],
				mWorldTransform.Position.z + UIoffset.m128_f32[2]);
			XMMATRIX S = XMMatrixScaling(
				mWorldTransform.Scale.x, 
				mWorldTransform.Scale.y, 
				mWorldTransform.Scale.z);

			// Background Health Bar
			if (e->ObjCBIndex % 2 == 1)
			{
				T = XMMatrixTranslation(
					mWorldTransform.Position.x, 
					mWorldTransform.Position.y, 
					mWorldTransform.Position.z);
				S = XMMatrixIdentity();
			}

			XMMATRIX world = XMLoadFloat4x4(&e->World) * playerWorld * T;
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			UIConstants uiConstants;
			XMStoreFloat4x4(&uiConstants.World, XMMatrixTranspose(world) * S);
			XMStoreFloat4x4(&uiConstants.TexTransform, XMMatrixTranspose(texTransform));

			curUICB->CopyData(e->ObjCBIndex, uiConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

