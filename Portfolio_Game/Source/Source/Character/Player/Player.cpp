#include "Player.h"
#include "GameTimer.h"

using namespace DirectX;

Player::Player()
	: Character(),
	mPlayerMovement(),
	mClipName("Idle")
{
}


Player::~Player()
{

}

void Player::SetClipName(const std::string& inClipName)
{
	mClipName = inClipName;
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

bool Player::isClipEnd()
{
	return Character::isClipEnd(mClipName);
}

void Player::PlayerMove(PlayerMoveList move, float velocity)
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

void Player::UpdateCharacterCBs(FrameResource* mCurrFrameResource, const Light& mMainLight, RenderLayer type, const GameTimer & gt)
{
	Character::UpdateCharacterCBs(mCurrFrameResource->PlayerCB, mMainLight, type, mClipName, gt);

	mCamera.UpdateViewMatrix();

	XMVECTOR Trnaslation = 0.9 * XMVectorSubtract(mCamera.GetEyePosition(), mPlayerMovement.GetPlayerPosition());
	// UI
	auto currUICB = mCurrFrameResource->UICB.get();
	mUI.UpdateUICBs(currUICB, XMLoadFloat4x4(&GetWorld()) * XMMatrixTranslationFromVector(Trnaslation), mTransformDirty);
}

void Player::UpdateTransformationMatrix()
{
	XMMATRIX world;

	if (mPlayerMovement.UpdateTransformationMatrix(world))
	{
		SetWorldTransform(world);
	}
}
