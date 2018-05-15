#include "RenderItem.h"
#include "GameTimer.h"
#include "Player.h"

using namespace DirectX;

Player::Player()
	: Character(),
	mPlayerInfo(),
	mFullHealth(100),
	mDamage(10)
{
	XMVECTOR P = XMVectorSet(-180.0f, 0.0f, -170.0f, 0.0f);
	mPlayerInfo.mMovement.SetPlayerPosition(P);
}

Player::~Player()
{

}


int Player::GetHealth(int i) const
{
	return mPlayerInfo.mHealth;
}

CharacterInfo & Player::GetCharacterInfo(int cIndex)
{
	return mPlayerInfo;
}

void Player::Damage(int damage, XMVECTOR Position, XMVECTOR Look)
{
	XMVECTOR mP = Position;
	XMVECTOR P = mPlayerInfo.mMovement.GetPlayerPosition();

	if (MathHelper::getDistance(mP, P) > 15.0f)
		return;

	if (mPlayerInfo.mHealth >= 0)
	{
		mSkinnedModelInst->TimePos = 0.0f;
	}
	else
	{
		return;
	}

	SetClipName("HitReaction");
	mPlayerInfo.mHealth -= damage;

	mUI.SetDamageScale(static_cast<float>(mPlayerInfo.mHealth) / static_cast<float>(mFullHealth));
}

void Player::Attack(Character * inMonster, std::string clipName)
{
	SetClipName(clipName);
	SetClipTime(0.0f);

	if (clipName == "Hook")
		mDamage = 10;
	else if (clipName == "Kick")
		mDamage = 20;
	else if (clipName == "Kick2")
		mDamage = 30;

	inMonster->Damage(
		mDamage,
		mPlayerInfo.mMovement.GetPlayerPosition(),
		mPlayerInfo.mMovement.GetPlayerLook());
}

bool Player::isClipEnd()
{
	auto clipEndTime = mSkinnedInfo.GetAnimation(mPlayerInfo.mClipName).GetClipEndTime();
	auto curTimePos = mSkinnedModelInst->TimePos;
	if (clipEndTime - curTimePos < 0.001f)
		return true;
	return false;
}

eClipList Player::GetCurrentClip() const
{
	return mSkinnedModelInst->mState;
}

XMMATRIX Player::GetWorldTransformMatrix() const
{
	auto T = mPlayerInfo.mMovement.GetWorldTransformInfo();
	XMMATRIX P = XMMatrixTranslation(T.Position.x, T.Position.y, T.Position.z);
	XMMATRIX R = XMLoadFloat4x4(&T.Rotation);
	XMMATRIX S = XMMatrixScaling(T.Scale.x, T.Scale.y, T.Scale.z);

	return  S * R * P;
}

UINT Player::GetAllRitemsSize() const
{
	return (UINT)mAllRitems.size();
}

const std::vector<RenderItem*> Player::GetRenderItem(RenderLayer Type) const
{
	return mRitems[(int)Type];
}


void Player::SetClipName(const std::string& inClipName)
{
	if (mPlayerInfo.mClipName != "Death")
	{
		mPlayerInfo.mClipName = inClipName;
	}
}

void Player::SetClipTime(float time)
{
	mSkinnedModelInst->TimePos = time;
}


void Player::BuildGeometry(
	ID3D12Device * device,
	ID3D12GraphicsCommandList* cmdList,
	const std::vector<CharacterVertex>& inVertices,
	const std::vector<std::uint32_t>& inIndices,
	const SkinnedData& inSkinInfo,
	std::string geoName)
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
	vCount = (UINT)inVertices.size();
	iCount = (UINT)inIndices.size();

	const UINT vbByteSize = (UINT)inVertices.size() * sizeof(CharacterVertex);
	const UINT ibByteSize = (UINT)inIndices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = geoName;

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), inVertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), inIndices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, inVertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, inIndices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(CharacterVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	auto vSubmeshOffset = mSkinnedInfo.GetSubmeshOffset();
	auto vBoneName = mSkinnedInfo.GetBoneName();


	UINT SubmeshOffsetIndex = 0;
	for (int i = 0; i < vSubmeshOffset.size(); ++i)
	{
		UINT CurrSubmeshOffsetIndex = vSubmeshOffset[i];

		SubmeshGeometry FbxSubmesh;
		FbxSubmesh.IndexCount = CurrSubmeshOffsetIndex;
		FbxSubmesh.StartIndexLocation = SubmeshOffsetIndex;
		FbxSubmesh.BaseVertexLocation = 0;

		std::string SubmeshName = vBoneName[i];
		geo->DrawArgs[SubmeshName] = FbxSubmesh;

		SubmeshOffsetIndex += CurrSubmeshOffsetIndex;
	}

	BoundingBox box;
	BoundingBox::CreateFromPoints(
		box,
		inVertices.size(),
		&inVertices[0].Pos,
		sizeof(CharacterVertex));
	box.Extents = { 3.0f, 1.0f, 3.0f };
	box.Center.y = 3.0f;

	mInitBoundsBox = box;
	mPlayerInfo.mBoundingBox = box;

	mGeometry = std::move(geo);
}

void Player::BuildConstantBufferViews(
	ID3D12Device * device,
	ID3D12DescriptorHeap * mCbvHeap,
	const std::vector<std::unique_ptr<FrameResource>>& mFrameResources,
	int mPlayerCbvOffset)
{
	UINT playerCount = GetAllRitemsSize();
	UINT playerCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(CharacterConstants));
	UINT mCbvSrvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto playerCB = mFrameResources[frameIndex]->PlayerCB->Resource();

		for (UINT i = 0; i < playerCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = playerCB->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * playerCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = mPlayerCbvOffset + frameIndex * playerCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = playerCBByteSize;

			device->CreateConstantBufferView(&cbvDesc, handle);
		}
	}
}

void Player::BuildRenderItem(
	Materials& mMaterials,
	std::string matrialPrefix)
{
	int playerIndex = 0;
	int boneCount = (UINT)mSkinnedInfo.BoneCount();
	auto vBoneName = mSkinnedInfo.GetBoneName();

	// Character Mesh
	for (int submeshIndex = 0; submeshIndex < boneCount - 1; ++submeshIndex)
	{
		std::string SubmeshName = vBoneName[submeshIndex];

		auto PlayerRitem = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&PlayerRitem->World, XMMatrixScaling(4.0f, 4.0f, 4.0f));
		PlayerRitem->TexTransform = MathHelper::Identity4x4();
		PlayerRitem->Mat = mMaterials.Get(matrialPrefix);
		PlayerRitem->Geo = mGeometry.get();
		PlayerRitem->NumFramesDirty = gNumFrameResources;
		PlayerRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		PlayerRitem->StartIndexLocation = PlayerRitem->Geo->DrawArgs[SubmeshName].StartIndexLocation;
		PlayerRitem->BaseVertexLocation = PlayerRitem->Geo->DrawArgs[SubmeshName].BaseVertexLocation;
		PlayerRitem->IndexCount = PlayerRitem->Geo->DrawArgs[SubmeshName].IndexCount;
		PlayerRitem->SkinnedModelInst = mSkinnedModelInst.get();
		PlayerRitem->PlayerCBIndex = playerIndex++;

		auto ShadowedRitem = std::make_unique<RenderItem>();
		*ShadowedRitem = *PlayerRitem;
		ShadowedRitem->Mat = mMaterials.Get("shadow0");
		ShadowedRitem->NumFramesDirty = gNumFrameResources;
		ShadowedRitem->SkinnedModelInst = mSkinnedModelInst.get();
		ShadowedRitem->PlayerCBIndex = playerIndex++;

		mRitems[(int)RenderLayer::Character].push_back(PlayerRitem.get());
		mAllRitems.push_back(std::move(PlayerRitem));
		mRitems[(int)RenderLayer::Shadow].push_back(ShadowedRitem.get());
		mAllRitems.push_back(std::move(ShadowedRitem));
	}
}


void Player::UpdateCharacterCBs(
	FrameResource* mCurrFrameResource,
	const Light& mMainLight,
	float* Delay,
	const GameTimer & gt)
{
	if (mPlayerInfo.mHealth <= 0 && mPlayerInfo.mClipName != "Death")
	{
		SetClipName("Death");
		mSkinnedModelInst->TimePos = 0.0f;
	}
	mSkinnedModelInst->UpdateSkinnedAnimation(mPlayerInfo.mClipName, gt.DeltaTime());

	auto currPlayerCB = mCurrFrameResource->PlayerCB.get();
	for (auto& e : mRitems[(int)RenderLayer::Character])
	{

		CharacterConstants skinnedConstants;

		std::copy(
			std::begin(mSkinnedModelInst->FinalTransforms),
			std::end(mSkinnedModelInst->FinalTransforms),
			&skinnedConstants.BoneTransforms[0]);

		XMMATRIX world = XMLoadFloat4x4(&e->World) * GetWorldTransformMatrix();
		XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

		XMStoreFloat4x4(&skinnedConstants.World, XMMatrixTranspose(world));
		XMStoreFloat4x4(&skinnedConstants.TexTransform, XMMatrixTranspose(texTransform));

		currPlayerCB->CopyData(e->PlayerCBIndex, skinnedConstants);
	}

	mInitBoundsBox.Transform(mPlayerInfo.mBoundingBox, GetWorldTransformMatrix());

	UpdateCharacterShadows(mMainLight);
	for (auto& e : mRitems[(int)RenderLayer::Shadow])
	{

		CharacterConstants skinnedConstants;

		std::copy(
			std::begin(mSkinnedModelInst->FinalTransforms),
			std::end(mSkinnedModelInst->FinalTransforms),
			&skinnedConstants.BoneTransforms[0]);

		XMMATRIX world = XMLoadFloat4x4(&e->World);
		XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

		XMStoreFloat4x4(&skinnedConstants.World, XMMatrixTranspose(world));
		XMStoreFloat4x4(&skinnedConstants.TexTransform, XMMatrixTranspose(texTransform));

		currPlayerCB->CopyData(e->PlayerCBIndex, skinnedConstants);
	}

	mCamera.UpdateViewMatrix();

	XMVECTOR translation = 0.9 * XMVectorSubtract(mCamera.GetEyePosition(), mPlayerInfo.mMovement.GetPlayerPosition());
	mUI.SetPosition(translation);

	// UI
	auto curUICB = mCurrFrameResource->UICB.get();
	mUI.UpdateUICBs(
		curUICB,
		GetWorldTransformMatrix(),
		-mPlayerInfo.mMovement.GetPlayerRight(),
		Delay,
		mTransformDirty);
}

void Player::UpdateCharacterShadows(const Light& mMainLight)
{
	int i = 0;
	for (auto& e : mRitems[(int)RenderLayer::Shadow])
	{
		// Load the object world
		auto& o = mRitems[(int)RenderLayer::Character][i];

		XMMATRIX shadowWorld = XMLoadFloat4x4(&o->World) * GetWorldTransformMatrix();
		XMVECTOR shadowPlane = XMVectorSet(0.0f, 0.1f, 0.0f, 0.0f);
		XMVECTOR toMainLight = -XMLoadFloat3(&mMainLight.Direction);
		XMMATRIX S = XMMatrixShadow(shadowPlane, toMainLight);
		XMMATRIX shadowOffsetY = XMMatrixTranslation(0.0f, 0.001f, 0.0f);
		XMStoreFloat4x4(&e->World, shadowWorld * S * shadowOffsetY);

		++i;
	}
}

void Player::UpdatePlayerPosition(PlayerMoveList moveName, float velocity)
{

	switch (moveName)
	{
	case PlayerMoveList::Walk:
		mPlayerInfo.mMovement.Walk(velocity);
		break;

	case PlayerMoveList::SideWalk:
		mPlayerInfo.mMovement.SideWalk(velocity);
		break;

	case PlayerMoveList::AddYaw:
		mPlayerInfo.mMovement.AddYaw(velocity);
		break;

	case PlayerMoveList::AddPitch:
		mPlayerInfo.mMovement.AddPitch(velocity);
		break;
	case PlayerMoveList::Death:

		break;
	}

	mCamera.UpdatePosition(
		mPlayerInfo.mMovement.GetPlayerPosition(),
		mPlayerInfo.mMovement.GetPlayerLook(),
		mPlayerInfo.mMovement.GetPlayerUp(),
		mPlayerInfo.mMovement.GetPlayerRight());

	mTransformDirty = true;
}

void Player::UpdateTransformationMatrix()
{
	mPlayerInfo.mMovement.UpdateTransformationMatrix();
}