#include "Player.h"
#include "GameTimer.h"

using namespace DirectX;

Player::Player()
	: Character(),
	mPlayerMovement(),
	mClipName("Idle"),
	mHealth(100),
	fullHealth(100)
{
}

Player::~Player()
{

}

UINT Player::GetHealth() const
{
	return mHealth;
}
void Player::Damage(int damage, int cIndex)
{
	mHealth -= damage;
	mClipName = "HitReaction";
	
	mUI.SetDamageScale(-mPlayerMovement.GetPlayerRight(), static_cast<float>(mHealth) / static_cast<float>(fullHealth));
	mSkinnedModelInst->TimePos = 0.0f;
}
UINT Player::GetAllRitemsSize() const
{
	return (UINT)mAllRitems.size();
}
UINT Player::GetBoneSize() const
{
	return (UINT)mSkinnedInfo.BoneCount();
}
eClipList Player::GetCurrentClip() const
{
	return mSkinnedModelInst->state;
}
WorldTransform& Player::GetWorldTransform(int i)
{
	return mWorldTransform;
}
DirectX::XMFLOAT4X4 Player::GetWorldTransform4x4f() const
{
	auto T = mWorldTransform;
	XMMATRIX P = XMMatrixTranslation(T.Position.x, T.Position.y, T.Position.z);
	XMMATRIX R = XMLoadFloat4x4(&T.Rotation);
	XMMATRIX S = XMMatrixScaling(T.Scale.x, T.Scale.y, T.Scale.z);

	XMFLOAT4X4 Ret;
	XMStoreFloat4x4(&Ret, S * R * P);
	return Ret;
}
const std::vector<RenderItem*> Player::GetRenderItem(RenderLayer Type) const
{
	return mRitems[(int)Type];
}

void Player::SetClipName(const std::string& inClipName)
{
	if (mClipName != "Death")
	{
		mClipName = inClipName;
	}
}
void Player::SetClipTime(float time)
{
	mSkinnedModelInst->TimePos = time;
}
bool Player::isClipEnd()
{
	if (mSkinnedInfo.GetAnimation(mClipName).GetClipEndTime() - mSkinnedModelInst->TimePos < 0.001f)
		return true;
	return false;
}

void Player::BuildGeometry(ID3D12Device * device, ID3D12GraphicsCommandList* cmdList, const std::vector<SkinnedVertex>& inVertices, const std::vector<std::uint32_t>& inIndices, const SkinnedData& inSkinInfo, std::string geoName)
{
	mSkinnedInfo = inSkinInfo;

	mSkinnedModelInst = std::make_unique<SkinnedModelInstance>();
	mSkinnedModelInst->SkinnedInfo = &mSkinnedInfo;
	mSkinnedModelInst->FinalTransforms.resize(mSkinnedInfo.BoneCount());
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
	const UINT ibByteSize = (UINT)inIndices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = geoName;

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), inVertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), inIndices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, inVertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, inIndices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(SkinnedVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	auto vSubmeshOffset = mSkinnedInfo.GetSubmeshOffset();
	auto boneName = mSkinnedInfo.GetBoneName();

	UINT SubmeshOffsetIndex = 0;
	for (int i = 0; i < vSubmeshOffset.size(); ++i)
	{
		UINT CurrSubmeshOffsetIndex = vSubmeshOffset[i];

		SubmeshGeometry FbxSubmesh;
		FbxSubmesh.IndexCount = CurrSubmeshOffsetIndex;
		FbxSubmesh.StartIndexLocation = SubmeshOffsetIndex;
		FbxSubmesh.BaseVertexLocation = 0;

		std::string SubmeshName = boneName[i];
		geo->DrawArgs[SubmeshName] = FbxSubmesh;

		SubmeshOffsetIndex += CurrSubmeshOffsetIndex;
	}

	mGeometry = std::move(geo);
}
void Player::BuildRenderItem(Materials& mMaterials, std::string matrialPrefix)
{
	int chaIndex = 0;
	int BoneCount = GetBoneSize();
	auto boneName = mSkinnedInfo.GetBoneName();

		WorldTransform wTransform;
		wTransform.Position = { 0.0f, 0.0f, 0.0f };
		wTransform.Scale = { 1.0f, 1.0f, 1.0f };
		wTransform.Look = { 0.0f, 0.0f, 1.0f };
		XMStoreFloat4x4(&wTransform.Rotation, XMMatrixRotationRollPitchYaw(0.0f, 0.0f, 0.0f));

		// Character Mesh
		for (int submeshIndex = 0; submeshIndex < BoneCount - 1; ++submeshIndex)
		{
			std::string SubmeshName = boneName[submeshIndex];
			std::string MaterialName = matrialPrefix; // TODO : Setting the Name

			auto FbxRitem = std::make_unique<RenderItem>();
			XMStoreFloat4x4(&FbxRitem->World, XMMatrixScaling(4.0f, 4.0f, 4.0f));
			FbxRitem->TexTransform = MathHelper::Identity4x4();
			FbxRitem->Mat = mMaterials.Get(MaterialName);
			FbxRitem->Geo = mGeometry.get();
			FbxRitem->NumFramesDirty = gNumFrameResources;
			FbxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			FbxRitem->StartIndexLocation = FbxRitem->Geo->DrawArgs[SubmeshName].StartIndexLocation;
			FbxRitem->BaseVertexLocation = FbxRitem->Geo->DrawArgs[SubmeshName].BaseVertexLocation;
			FbxRitem->IndexCount = FbxRitem->Geo->DrawArgs[SubmeshName].IndexCount;
			FbxRitem->SkinnedModelInst = mSkinnedModelInst.get();
			FbxRitem->PlayerCBIndex = chaIndex++;

			auto shadowedObjectRitem = std::make_unique<RenderItem>();
			*shadowedObjectRitem = *FbxRitem;
			shadowedObjectRitem->Mat = mMaterials.Get("shadow0");
			shadowedObjectRitem->NumFramesDirty = gNumFrameResources;
			shadowedObjectRitem->SkinnedModelInst = mSkinnedModelInst.get();
			shadowedObjectRitem->PlayerCBIndex = chaIndex++;

			mRitems[(int)RenderLayer::Character].push_back(FbxRitem.get());
			mAllRitems.push_back(std::move(FbxRitem));
			mRitems[(int)RenderLayer::Shadow].push_back(shadowedObjectRitem.get());
			mAllRitems.push_back(std::move(shadowedObjectRitem));
		}
		mWorldTransform = wTransform;
}
void Player::BuildConstantBufferViews(ID3D12Device * device, ID3D12DescriptorHeap * mCbvHeap, const std::vector<std::unique_ptr<FrameResource>>& mFrameResources, int mChaCbvOffset)
{
	UINT characterCount = GetAllRitemsSize();
	UINT chaCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(SkinnedConstants));
	UINT mCbvSrvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto characterCB = mFrameResources[frameIndex]->PlayerCB->Resource();

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

void Player::UpdatePlayerPosition(const Character& Monster, PlayerMoveList move, float velocity)
{
	switch (move)
	{
	case PlayerMoveList::Walk :
		mPlayerMovement.Walk(velocity);
		break;

	case PlayerMoveList::SideWalk :
		mPlayerMovement.SideWalk(velocity);
		break;

	case PlayerMoveList::AddYaw :
		mPlayerMovement.AddYaw(velocity);
		break;
		
	case PlayerMoveList::AddPitch :
		mPlayerMovement.AddPitch(velocity);
		break;
	}

	mCamera.UpdatePosition(
		mPlayerMovement.GetPlayerPosition(),
		mPlayerMovement.GetPlayerLook(),
		mPlayerMovement.GetPlayerUp(),
		mPlayerMovement.GetPlayerRight());

	mTransformDirty = true;
}

void Player::UpdateCharacterCBs(FrameResource* mCurrFrameResource, const Light& mMainLight, const GameTimer & gt)
{
	auto currCharacterCB = mCurrFrameResource->PlayerCB.get();
	UpdateCharacterShadows(mMainLight);

	mSkinnedModelInst->UpdateSkinnedAnimation(mClipName, gt.DeltaTime());
	for (auto& e : mAllRitems)
	{
		if (mTransformDirty) { e->NumFramesDirty = gNumFrameResources; }
		if (e->NumFramesDirty > 0)
		{
			SkinnedConstants skinnedConstants;

			std::copy(
				std::begin(mSkinnedModelInst->FinalTransforms),
				std::end(mSkinnedModelInst->FinalTransforms),
				&skinnedConstants.BoneTransforms[0]);

			// TODO : player constroller
			XMMATRIX world = XMLoadFloat4x4(&e->World) * XMLoadFloat4x4(&GetWorldTransform4x4f());
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			XMStoreFloat4x4(&skinnedConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&skinnedConstants.TexTransform, XMMatrixTranspose(texTransform));

			currCharacterCB->CopyData(e->PlayerCBIndex, skinnedConstants);

			// Next FrameResource need to be updated too.

			// TODO:
			//e->NumFramesDirty--;
		}
	}

	mCamera.UpdateViewMatrix();

	XMVECTOR Translation = 0.9 * XMVectorSubtract(mCamera.GetEyePosition(), mPlayerMovement.GetPlayerPosition());
	mUI.SetPosition(Translation);

	// UI
	auto currUICB = mCurrFrameResource->UICB.get();
	mUI.UpdateUICBs(currUICB, XMLoadFloat4x4(&GetWorldTransform4x4f()), mTransformDirty);
}
void Player::UpdateTransformationMatrix()
{
	WorldTransform world = GetWorldTransform();

	if (mPlayerMovement.UpdateTransformationMatrix(world))
	{
		mWorldTransform = world;
	}
}
void Player::UpdateCharacterShadows(const Light& mMainLight)
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

		++i;
	}
}
