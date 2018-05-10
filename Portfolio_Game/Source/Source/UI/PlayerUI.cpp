#include "Materials.h"
#include "PlayerUI.h"

using namespace DirectX;

PlayerUI::PlayerUI()
{
	mWorldTransform.Scale = { 1.0f, 1.0f, 1.0f };
	mWorldTransform.Position = { 0.0f, 0.0f, 0.0f };
}


PlayerUI::~PlayerUI()
{
}

UINT PlayerUI::GetSize() const
{
	return (UINT)mAllRitems.size();
}
const std::vector<RenderItem*> PlayerUI::GetRenderItem(eUIList Type)
{
	return mRitems[(int)Type];
}

void PlayerUI::SetPosition(FXMVECTOR inPosition)
{
	XMStoreFloat3(&mWorldTransform.Position, inPosition);
}
void PlayerUI::SetDamageScale(float inScale)
{
	mWorldTransform.Scale.x = inScale;
}

void PlayerUI::BuildConstantBufferViews(
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

void PlayerUI::BuildGeometry(
	ID3D12Device * device,
	ID3D12GraphicsCommandList* cmdList,
	const std::vector<UIVertex>& inVertices,
	const std::vector<std::uint32_t>& inIndices,
	std::string geoName)
{
	UINT vCount = 0, iCount = 0;
	vCount = inVertices.size();
	iCount = inIndices.size();

	const UINT vbByteSize = (UINT)inVertices.size() * sizeof(UIVertex);
	const UINT ibByteSize = (UINT)inIndices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = geoName;

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), inVertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), inIndices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, inVertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, inIndices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(UIVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry UISubmesh;
	UISubmesh.IndexCount = (UINT)inIndices.size();
	UISubmesh.StartIndexLocation = 0;
	UISubmesh.BaseVertexLocation = 0;

	geo->DrawArgs["SkillUI"] = UISubmesh;

	mGeometry = std::move(geo);
}

void PlayerUI::BuildRenderItem(std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>& mGeometries, Materials & mMaterials)
{
	int UIIndex = 0;
	// TODO : Setting the Name

	// place over the head
	auto frontHealthBar = std::make_unique<RenderItem>();
	// atan(theta) : Theta is associated with PlayerCamera
	XMStoreFloat4x4(&frontHealthBar->World, XMMatrixScaling(0.01f, 1.0f, 0.0021f) * XMMatrixRotationX(-atan(3.0f / 2.0f)) * XMMatrixTranslation(0.0f, 0.895f, 0.0f));
	frontHealthBar->TexTransform = MathHelper::Identity4x4();
	frontHealthBar->Mat = mMaterials.Get("ice0");
	frontHealthBar->Geo = mGeometries["shapeGeo"].get();
	frontHealthBar->ObjCBIndex = UIIndex++;
	frontHealthBar->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	frontHealthBar->StartIndexLocation = frontHealthBar->Geo->DrawArgs["hpBar"].StartIndexLocation;
	frontHealthBar->BaseVertexLocation = frontHealthBar->Geo->DrawArgs["hpBar"].BaseVertexLocation;
	frontHealthBar->IndexCount = frontHealthBar->Geo->DrawArgs["hpBar"].IndexCount;
	mRitems[(int)eUIList::Rect].push_back(frontHealthBar.get());
	mAllRitems.push_back(std::move(frontHealthBar));

	auto bgHealthBar = std::make_unique<RenderItem>();
	// atan(theta) : Theta is associated with PlayerCamera
	XMStoreFloat4x4(&bgHealthBar->World, XMMatrixScaling(0.01f, 1.0f, 0.002f) * XMMatrixRotationX(-atan(3.0f / 2.0f))  * XMMatrixTranslation(0.0f, 0.89f, 0.05f));
	bgHealthBar->TexTransform = MathHelper::Identity4x4();
	bgHealthBar->Mat = mMaterials.Get("bricks0");
	bgHealthBar->Geo = mGeometries["shapeGeo"].get();
	bgHealthBar->ObjCBIndex = UIIndex++;
	bgHealthBar->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	bgHealthBar->StartIndexLocation = bgHealthBar->Geo->DrawArgs["hpBar"].StartIndexLocation;
	bgHealthBar->BaseVertexLocation = bgHealthBar->Geo->DrawArgs["hpBar"].BaseVertexLocation;
	bgHealthBar->IndexCount = bgHealthBar->Geo->DrawArgs["hpBar"].IndexCount;
	mRitems[(int)eUIList::Rect].push_back(bgHealthBar.get());
	mAllRitems.push_back(std::move(bgHealthBar));

	// Skill Icon
	auto iconDelayKick = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&iconDelayKick->World, XMMatrixScaling(0.01f, 1.0f, 0.01f) * XMMatrixRotationX(-atan(3.0f / 2.0f))  * XMMatrixTranslation(-0.05f, -0.1f, 0.0f));
	iconDelayKick->TexTransform = MathHelper::Identity4x4();
	iconDelayKick->Mat = mMaterials.Get("iconPunch");
	iconDelayKick->Geo = mGeometry.get();
	iconDelayKick->ObjCBIndex = UIIndex++;
	iconDelayKick->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	iconDelayKick->StartIndexLocation = iconDelayKick->Geo->DrawArgs["SkillUI"].StartIndexLocation;
	iconDelayKick->BaseVertexLocation = iconDelayKick->Geo->DrawArgs["SkillUI"].BaseVertexLocation;
	iconDelayKick->IndexCount = iconDelayKick->Geo->DrawArgs["SkillUI"].IndexCount;
	mRitems[(int)eUIList::I_Punch].push_back(iconDelayKick.get());
	mAllRitems.push_back(std::move(iconDelayKick));
	skillFullTime.push_back(3.0f);


	auto iconKick = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&iconKick->World, XMMatrixScaling(0.01f, 1.0f, 0.01f) * XMMatrixRotationX(-atan(3.0f / 2.0f))  * XMMatrixTranslation(0.05f, -0.1f, 0.0f));
	iconKick->TexTransform = MathHelper::Identity4x4();
	iconKick->Mat = mMaterials.Get("iconKick");
	iconKick->Geo = mGeometry.get();
	iconKick->ObjCBIndex = UIIndex++;
	iconKick->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	iconKick->StartIndexLocation = iconKick->Geo->DrawArgs["SkillUI"].StartIndexLocation;
	iconKick->BaseVertexLocation= iconKick->Geo->DrawArgs["SkillUI"].BaseVertexLocation;
	iconKick->IndexCount = iconKick->Geo->DrawArgs["SkillUI"].IndexCount;
	mRitems[(int)eUIList::I_Kick].push_back(iconKick.get());
	mAllRitems.push_back(std::move(iconKick));
	skillFullTime.push_back(5.0f);

}

void PlayerUI::UpdateUICBs(
	UploadBuffer<UIConstants>* curUICB,
	XMMATRIX playerWorld,
	XMVECTOR inEyeLeft,
	std::vector<float> Delay,
	bool mTransformDirty)
{
	int iconIndex = 0;
	for (auto& e : mAllRitems)
	{
		// if Transform then Reset the Dirty flag
		if (mTransformDirty) { e->NumFramesDirty = gNumFrameResources; }

		if (e->NumFramesDirty > 0)
		{
			// Health Bar move to left
			XMVECTOR UIoffset = (1.0f- mWorldTransform.Scale.x) * inEyeLeft * 0.1f;

			XMMATRIX T = XMMatrixTranslation(
				mWorldTransform.Position.x,
				mWorldTransform.Position.y,
				mWorldTransform.Position.z);
			XMMATRIX S = XMMatrixIdentity();
			// Background Health Bar
			if (e->ObjCBIndex < 0)
			{
				T = XMMatrixTranslation(
					mWorldTransform.Position.x + UIoffset.m128_f32[0],
					mWorldTransform.Position.y + UIoffset.m128_f32[1],
					mWorldTransform.Position.z + UIoffset.m128_f32[2]);
				S = XMMatrixScaling(
					mWorldTransform.Scale.x,
					mWorldTransform.Scale.y,
					mWorldTransform.Scale.z);
			}

			XMMATRIX world = XMLoadFloat4x4(&e->World) * playerWorld * T;
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);
			UIConstants uiConstants;
			XMStoreFloat4x4(&uiConstants.World, XMMatrixTranspose(world) * S);
			XMStoreFloat4x4(&uiConstants.TexTransform, XMMatrixTranspose(texTransform));

			float remainingTime;
			if (iconIndex > 1)
			{
				// iconIndex - 2 -> health Bar and bg bar
				remainingTime = skillFullTime[iconIndex - 2] - Delay[iconIndex];
				if (remainingTime < 0.0f) remainingTime = 0.0f;
				uiConstants.Scale = remainingTime / skillFullTime[iconIndex - 2];
			}

			curUICB->CopyData(e->ObjCBIndex, uiConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
		iconIndex++;
	}
}

