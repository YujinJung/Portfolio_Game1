#include "../Common/GameTimer.h"
#include "Player.h"

using namespace DirectX;

Player::Player()
	: Character(),
	mPlayerMovement()
	/*mCamera(
		XMFLOAT3(0.0f, 0.0f, 0.0f),
		XMFLOAT3(0.0f, 0.0f, 1.0f),
		XMFLOAT3(0.0f, 1.0f, 0.0f),
		XMFLOAT3(1.0f, 0.0f, 0.0f)
	)*/
{
	
}


Player::~Player()
{

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

void Player::UpdateCharacterCBs(FrameResource* mCurrFrameResource, const Light& mMainLight, const GameTimer & gt)
{
	Character::UpdateCharacterCBs(mCurrFrameResource, mMainLight, gt);

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
